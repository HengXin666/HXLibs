#include <HXLibs/net/Api.hpp>
#include <HXLibs/net/protocol/websocket/WebSocket.hpp>

using namespace HX;
using namespace net;
using namespace utils;

#include <iostream>
#include <list>

auto __init__ = []{
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        std::cout << "当前工作路径是: " << cwd << '\n';
        std::filesystem::current_path("../../../../");
        std::cout << "切换到路径: " << std::filesystem::current_path() << '\n';
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
    return 0;
}();

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

struct Test {
    auto before(Request&, Response&) {
        CHECK(true);
        return 1;
    }

    auto after(Request&, Response&) {
        CHECK(true);
        return true;
    }
};

// WebSocket 消息池
// 正确使用应该是 一个 ws 接口专门用来发送消息
// 而发送数据则应该使用另一个接口, 也就是不能使用这个ws
struct WSPool {
    coroutine::Task<> sendAll(std::string_view msg) {
        if (wsPool.empty())
            co_return;
        // 一次生成数据
        auto pk = WebSocketFactory::makePacketView(OpCode::Text, msg);
        for (auto& ws : wsPool) {
            // 多次重发这个数据
            co_await ws.sendPacketView(pk);
        }
    }

    std::list<WebSocket<WebSocketModel::Server>> wsPool;
};

TEST_CASE("测试普通请求") {
    HttpServer serv{"127.0.0.1", "28205",};

    WSPool pool;

    serv.addEndpoint<GET, POST>("/", [](
            Request& req,
            Response& res
        ) -> coroutine::Task<> {
            (void)req;
            co_await res.setStatusAndContent(
                Status::CODE_200, "<h1>Hello HXLibs</h1>")
                        .sendRes();
            co_return;
        }, Test{})
        .addEndpoint<GET>("/ws", [] ENDPOINT {
            auto ws = co_await WebSocketFactory::accept(req, res);
            struct JsonDataVo {
                std::string msg;
                int code;
            };
            JsonDataVo const vo{"Hello 客户端, 我只能通信3次!", 200};
            co_await ws.sendJson(vo);
            for (int i = 0; i < 3; ++i) {
                auto res = co_await ws.recvText();
                log::hxLog.info(res);
                co_await ws.sendText("Hello! " + res);
            }
            co_await ws.close();
            log::hxLog.info("断开ws");
            co_return ;
        })
        .addEndpoint<GET>("/ws_add_msg/{msg}", [&] ENDPOINT {
            // 群发内容
            auto msg = req.getPathParam(0);
            co_await pool.sendAll({msg.data(), msg.size()});
            co_await res.setResLine(HX::net::Status::CODE_200)
                        .sendRes();
            co_return;
        })
        .addEndpoint<GET>("/ws_send_poll", [&] ENDPOINT {
            auto ws = co_await WebSocketFactory::accept(req, res);
            auto it = pool.wsPool.emplace(pool.wsPool.end(), ws);
            try {
                while (true) {
                    // 仅维持心跳, 客户端不应该发送除了 ping 以外的任何内容
                    co_await ws.recvText();
                }
            } catch (...) {
                // ws连接已断开
            }
            // 注意线程安全啊! 这里是单线程, 仅示例, 故没有封装和上锁
            pool.wsPool.erase(it);
        })
    ;
    // serv.syncRun(1, 1500_ms); // 启动服务器
}