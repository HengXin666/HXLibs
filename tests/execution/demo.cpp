#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/coroutine/loop/ThreadLoop.hpp>
#include <HXLibs/container/ThreadPool.hpp>
#include <HXLibs/container/FutureResult.hpp>
#include <HXLibs/coroutine/task/Task.hpp>
#include <HXLibs/coroutine/awaiter/WhenAny.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

int main() {
    using namespace std::chrono;
    container::ThreadPool threadPool{};
    threadPool.run<container::ThreadPool::Model::FixedSizeAndNoCheck>();
    coroutine::EventLoop loop{};
    loop.sync([&]() -> coroutine::Task<> {
        log::hxLog.info("Hello Wow");
        auto twoTask = co_await ([&]() -> coroutine::Task<> {
            log::hxLog.info("萌萌滴干活!");
            co_await loop.makeTimer().sleepFor(3s);
        }() || loop.makeTimer().sleepFor(1s));
        log::hxLog.info("第", twoTask.index() + 1, "个任务完成了!");
        auto res = co_await threadPool.addTask([]{
            log::hxLog.info("sleep: 3s [begin]~");
            std::this_thread::sleep_for(3s);
            return 233;
        }).thenTry([](container::Try<int> t) {
            if (!t) {
                t.rethrow();
            }
            return 2233 + t.move();
        }).thenTry([](auto&& t) {
            if (!t) {
                t.rethrow();
            }
            return 2233 + t.move();
        }).via(loop);
        log::hxLog.info("sleep: 3s [end], val =", res);
        co_return;
    }());
}