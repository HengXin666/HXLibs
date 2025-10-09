#include <HXLibs/coroutine/awaiter/WhenAny.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/coroutine/task/RootTask.hpp>

#include <HXLibs/log/Log.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace HX;
using namespace std::chrono;

TEST_CASE("whenAny") {
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
    [[maybe_unused]] auto rootTask = [&]() -> coroutine::RootTask<> {
        log::hxLog.info("root Begin");
        co_await loop.makeTimer().sleepFor(50ms);
        log::hxLog.info("root End");
        co_return ;
    };

    static_assert(requires {
        coroutine::operator||(cTask01(), cTask02());
    }, "");

    // 显然不支持. 语法都不正确.
    // static_assert(!requires {
    //     coroutine::operator||(rootTask(), cTask02());
    // }, "");

    auto task = [&]() -> coroutine::Task<> {
        log::hxLog.debug("{");
        // co_await coroutine::whenAny(cTask01(), cTask02());
        rootTask().detach();
        auto res = co_await (cTask01() || (cTask02() || cTask01()));
        log::hxLog.debug("} idx:", res.index(), res);
        co_return ;
    };
    
    loop.sync(task());
}

TEST_CASE("02") {
    coroutine::EventLoop loop;

    // 延时不同，检查返回的 index 与类型
    auto cTask01 = [&]() -> coroutine::Task<int> {
        co_await loop.makeTimer().sleepFor(15ms);
        std::cout << "cTask01 done\n";
        co_return 1;
    };

    auto cTask02 = [&]() -> coroutine::Task<std::string> {
        co_await loop.makeTimer().sleepFor(10ms);
        std::cout << "cTask02 done\n";
        co_return std::string("02");
    };

    auto cTask03 = [&]() -> coroutine::Task<double> {
        co_await loop.makeTimer().sleepFor(5ms);
        std::cout << "cTask03 done\n";
        co_return 3.14;
    };

    // 测试链式组合: 单任务、混合类型、重复任务
    auto testTask = [&]() -> coroutine::Task<> {
        std::cout << "Start whenAny test\n";

        auto res1 = co_await (cTask01() || cTask02());
        std::cout << "Result 1 idx: " << res1.index() << "\n";

        auto res2 = co_await (cTask03() || (cTask01() || cTask02()));
        std::cout << "Result 2 idx: " << res2.index() << "\n";

        // 测试重复任务 & 类型混合
        auto res3 = co_await ((cTask01() || cTask01()) || (cTask02() || cTask03()));
        std::cout << "Result 3 idx: " << res3.index() << "\n";

        co_return;
    };

    loop.sync(testTask());
}