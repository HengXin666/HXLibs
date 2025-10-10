#include <HXLibs/coroutine/awaiter/WhenAll.hpp>
#include <HXLibs/coroutine/awaiter/WhenAny.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/coroutine/task/RootTask.hpp>

#include <HXLibs/log/Log.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace HX;
using namespace std::chrono;

TEST_CASE("whenAll") {
    coroutine::EventLoop loop;
    auto cTask01 = [&]() -> coroutine::Task<int> {
        co_await loop.makeTimer().sleepFor(10ms);
        log::hxLog.info("cTask01");
        co_return 1;
    };
    auto cTask02 = [&]() -> coroutine::Task<std::string> {
        co_await loop.makeTimer().sleepFor(9ms);
        log::hxLog.info("cTask02");
        co_return {"02"};
    };

    auto task = [&]() -> coroutine::Task<> {
        {
            log::hxLog.debug("{");
            auto [r1, r2] = co_await coroutine::whenAll(cTask01(), cTask02());
            log::hxLog.debug("}", r1, r2);
        }
        {
            log::hxLog.debug("{");
            auto [r1, r2, r3] = co_await (cTask01() && (cTask02() && cTask01()));
            log::hxLog.debug("}", r1, r2);
        }
        // 不支持熬~
        // {
        //     log::hxLog.debug("{");
        //     auto [r1, r2] = co_await (cTask01() && (cTask02() || cTask01()));
        //     log::hxLog.debug("}", r1, r2);
        // }
        co_return ;
    };
    
    loop.sync(task());
}