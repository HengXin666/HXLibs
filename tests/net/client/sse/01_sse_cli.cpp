#include <HXLibs/net/client/HttpClient.hpp>

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
    auto loop = std::make_shared<coroutine::EventLoop>();
    auto t = loop->trySync([&]() -> coroutine::Task<> {
        net::HttpClient cli{net::HttpClientOptions{}, loop};
        co_await cli.coSseLoop<net::GET>("http://127.0.0.1:8080/events", [](
            net::SseEvent sse
        ) -> coroutine::Task<> {
            log::hxLog.debug(sse.id, sse.event, ':', sse.data);
            co_return ;
        });
    }());
    if (!t) [[unlikely]] {
        log::hxLog.error(t.what());
    }
}
