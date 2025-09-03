#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/net/Api.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

int main() {
    setlocale(LC_ALL, "zh_CN.UTF-8");
    HttpServer server{"0.0.0.0", "28205"};
    using namespace utils;
    server.addEndpoint<GET>("/", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    }).addEndpoint<WS>("/ws", [] ENDPOINT {
        log::hxLog.warning("ws 端点");
        auto ws = co_await WebSocketFactory::accept(req, res);
        co_await ws.sendText("OK");
        co_await ws.close();
    });
    server.asyncRun(2, 100_ms); // 设置 0.1s 自动断开连接
    HttpClient client;
    log::hxLog.info("连接ing...");
    client.connect("http://0.0.0.0:28205/");
    log::hxLog.info("连接了~~~");
    std::this_thread::sleep_for(decltype(200_ms)::Val); // 等待 0.2s
    // 此时客户端已经被服务端断线了
    log::hxLog.info("连接ing...");
    client.connect("http://0.0.0.0:28205/");
    log::hxLog.info("连接了~~~");
    std::this_thread::sleep_for(decltype(200_ms)::Val); // 等待 0.2s
    client.wsLoop("ws://0.0.0.0:28205/ws", [](WebSocketClient ws) -> coroutine::Task<> {
        auto txt = co_await ws.recvText();
        log::hxLog.info("ws:", txt);
        co_await ws.close();
    }).thenTry([](container::Try<> t) {
        if (!t) [[unlikely]] {
            log::hxLog.error("ws:", t.what());
        }
    });
    return 0;
}