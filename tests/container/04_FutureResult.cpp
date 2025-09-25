#include <HXLibs/container/ThreadPool.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/net/Api.hpp>
#include <HXLibs/net/client/HttpClientPool.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace container;

TEST_CASE("测试协程的FutureResult") {
    using namespace std::chrono;
    coroutine::EventLoop loop;
    ThreadPool pool;
    pool.setFixedThreadNum(1);
    pool.run<ThreadPool::Model::FixedSizeAndNoCheck>();
    for (int i = 0; i < 2; ++i) {
        loop.trySync([&]() -> coroutine::Task<> {
            log::hxLog.debug("进入协程");
            auto res = co_await pool.addTask([] {
                log::hxLog.debug("等我500ms");
                std::this_thread::sleep_for(500ms);
                log::hxLog.info("ok");
                return 2233;
            }).via(loop);
            log::hxLog.info("返回:", res);
        }());
        log::hxLog.warning("等我1s");
        std::this_thread::sleep_for(1s);
    }
}

TEST_CASE("服务端协程FutureResult的使用") {
    using namespace net;
    using namespace std::chrono;
    HttpServer server{"127.0.0.1", "28205"};
    ThreadPool pool;
    pool.setFixedThreadNum(1);
    pool.run<ThreadPool::Model::FixedSizeAndNoCheck>();
    server.addEndpoint<WS>("/ws", [&] ENDPOINT {
        auto ws = co_await WebSocketFactory::accept(req, res);
        co_await ws.sendText("等我一下");
        auto ans = co_await pool.addTask([] {
            log::hxLog.debug("等我500ms");
            std::this_thread::sleep_for(500ms);
            log::hxLog.info("ok");
            return std::string{"你是谁?"};
        }).via(req.getIO());
        co_await ws.sendText("回复: " + ans);
        co_await ws.close();
    });
    server.asyncRun(1);
    HttpClientPool cli{2};
    auto r1 = cli.wsLoop("ws://127.0.0.1:28205/ws", [](WebSocketClient ws) -> coroutine::Task<> {
        auto msg = co_await ws.recvText();
        log::hxLog.info("他说:", msg);
        msg = co_await ws.recvText();
        log::hxLog.info("他终于说:", msg);
    });
    auto r2 = cli.wsLoop("ws://127.0.0.1:28205/ws", [](WebSocketClient ws) -> coroutine::Task<> {
        auto msg = co_await ws.recvText();
        log::hxLog.info("他说:", msg);
        msg = co_await ws.recvText();
        log::hxLog.info("他终于说:", msg);
    });
    r1.wait();
    r2.wait();
}