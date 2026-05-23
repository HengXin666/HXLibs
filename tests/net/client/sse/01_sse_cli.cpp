#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/net/ApiMacro.hpp>

using namespace HX;

int main() {
    /*
    接口: pass, 这种写法太操蛋了, 不如让内部直接这样. 反正你 co_await func(回调函数), 其实和下面也一样
    while (true) {
        auto event = co_await cli.makeSseStream();
        if (event.data == "[done]")
            break;
        // todo
    }

    这个接口
    co_await cli.coSseLoop<Post>([](SseEvent sse) -> Task<T> {})
    */
    net::HttpServer server{28205};
    server.addEndpoint<net::GET>("/events", [] ENDPOINT {
        using namespace utils::time_nttp_literals;
        auto sseStream = co_await res.makeSseStream();
        co_await ([&]() -> coroutine::Task<> {
            net::SseEvent event;
            for (int i = 0; i < 3; ++i) {
                co_await static_cast<coroutine::EventLoop&>(res.getIO())
                    .makeTimer()
                    .sleepFor(decltype(3_s)::StdChronoVal);
                event.data = (i & 1) ? "我是谁?\n是: 章鱼哥" : "我是谁?\n是: 花花公子帅章鱼";
                event.id = std::to_string(i);
                event.event = "who";
                event.retry = (100_ms).toChrono();
                co_await sseStream.push(event);
            }
        }() || [&]() -> coroutine::Task<> {
            while (true) {
                co_await sseStream.ping();
                co_await static_cast<coroutine::EventLoop&>(res.getIO())
                    .makeTimer()
                    .sleepFor(decltype(1_s)::StdChronoVal);
            }
        }());
        co_await res.getIO().close();
    });
    server.asyncRun(1);
    std::this_thread::sleep_for(std::chrono::seconds{1});

    auto loop = std::make_shared<coroutine::EventLoop>();
    auto t = loop->trySync([&]() -> coroutine::Task<> {
        net::HttpClient cli{net::HttpClientOptions{}, loop};
        co_await cli.coSseLoop<net::GET>("http://127.0.0.1:28205/events", [](
            net::SseEvent sse
        ) -> coroutine::Task<> {
            log::hxLog.info(sse.id, sse.event, ':', sse.data, "[", sse.retry, "]");
            co_return ;
        });
    }());
    if (!t) [[unlikely]] {
        log::hxLog.error(t.what());
    }
}
