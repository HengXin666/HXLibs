#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/net/Api.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;
using namespace utils;

using namespace std::chrono;

coroutine::Task<> coMain() {
    HttpClient cli{HttpClientOptions<decltype(3_s)>{}};
    log::hxLog.debug("开始请求");
    co_await cli.coWsLoop("ws://127.0.0.1:28205/ws",
        [](WebSocketClient ws) -> coroutine::Task<> {
            co_await ws.sendText("hello");
            auto msg = co_await ws.recvText();
            log::hxLog.info("收到: ", msg);
            co_await ws.close();
        }
    );
    try {
        co_await cli.coWsLoop("http://127.0.0.1:28205/ws",
            [](WebSocketClient ws) -> coroutine::Task<> {
                co_await ws.sendText("hello");
                auto msg = co_await ws.recvText();
                log::hxLog.info("收到: ", msg);
                co_await ws.close();
            }
        );
    } catch (std::runtime_error const& err) {
        log::hxLog.error("ws err:", err.what());
    }
    auto res = co_await cli.coGet("http://127.0.0.1:28205/ws");
    log::hxLog.info("res:", res);
    // 此处的 cli 已经被服务端断开连接了 ...
    co_return ;
}

int main() {
    HttpServer serv{"127.0.0.1", "28205"};
    serv.addEndpoint<GET>("/ws", [] ENDPOINT {
        auto head = req.getHeaders();
        log::hxLog.debug("[Server]: the: /ws", "head:", head);
        auto ws = co_await WebSocketFactory::accept(req, res);
        log::hxLog.debug("[Server]: 已成功建立 ws 连接!");
        while (true) {
            auto msg = co_await ws.recvText();
            co_await ws.sendText(std::move(msg) += " (HXLibs::Server 已阅)");
        }
        // 如果 ws 已断开, 那么会抛异常, 然后服务端会自动断开连接 (准确的说是close fd)
    });
    serv.asyncRun(1, 1500_ms);
    
    coMain().start();

    log::hxLog.warning("====== 测试同步 ======");

    HttpClient cli{HttpClientOptions<decltype(3_s)>{}};
    cli.wsLoop("ws://127.0.0.1:28205/ws",
        [](WebSocketClient ws) -> coroutine::Task<> {
            co_await ws.sendText("hello 我是同步接口的协程回调");
            auto msg = co_await ws.recvText();
            log::hxLog.info("收到: ", msg);
            co_await ws.close();
        }
    ).wait();
    
    std::this_thread::sleep_for(1s);
    log::hxLog.warning("一切都结束了..., 之后的连接是服务器为了快速关闭而RAII创建的客户端的请求");
    return 0;
}