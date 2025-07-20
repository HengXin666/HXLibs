#include <HXLibs/net/Api.hpp>
#include <HXLibs/net/protocol/websocket/WebSocket.hpp>

using namespace HX;
using namespace net;
using namespace utils;

#include <iostream>

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

TEST_CASE("测试普通请求") {
    HttpServer ser{"127.0.0.1", "28205",};
    ser
        .addEndpoint<GET, POST>("/", [](
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
            auto ws = co_await WebSocket::accept(req, res);
            co_await ws.send(OpCode::Text, "Hello! Main");
            for (int i = 0; i < 3; ++i) {
                auto res = co_await ws.recv();
                log::hxLog.info(res);
                co_await ws.send(OpCode::Text, "Hello! " + res);
            }
            co_await ws.close();
            log::hxLog.info("断开ws");
            co_return ;
        });
    // ser.syncRun(1, 1500_ms); // 启动服务器
}