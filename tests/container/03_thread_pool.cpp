#include <HXLibs/container/ThreadPool.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace container;

TEST_CASE("基本任务提交与获取结果") {
    ThreadPool pool;
    pool.run(std::chrono::milliseconds {1000}, ThreadPoolDefaultStrategy);

    auto res1 = pool.addTask([] { return 42; });
    auto res2 = pool.addTask([](int a, int b) { return a + b; }, 1, 2);
    auto res3 = pool.addTask([] { 
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
         return 99; 
    });
    int a = 0;
    auto res4 = pool.addTask([](int& a) { 
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
        return a = 99; 
    }, a);

    REQUIRE(res1.get() == 42);
    REQUIRE(res2.get() == 3);
    REQUIRE(res3.get() == 99);
    REQUIRE(res4.get() == a);  // 支持引用, 但是要自行处理好引用的生命周期

    // Error 可能会有生命周期问题!
    // {
    //     int a = 0;
    //     auto res4 = pool.addTask([](int& a) { 
    //         std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    //         return a = 99; 
    //     }, a);
    // }
    // <-- 如果在此次才开始 a = 99, 那么就是写入悬挂引用了!!!
}

TEST_CASE("基本任务提交与获取结果(固定容量版本)") {
    ThreadPool pool;
    pool.run<ThreadPool::Model::FixedSizeAndNoCheck>(
        std::chrono::milliseconds {1000}, ThreadPoolDefaultStrategy
    );

    auto res1 = pool.addTask([] { return 42; });
    auto res2 = pool.addTask([](int a, int b) { return a + b; }, 1, 2);
    auto res3 = pool.addTask([] { 
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 99; 
    });
    int a = 0;
    auto res4 = pool.addTask([](int& a) { 
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
        return a = 99; 
    }, a);
    auto res5 = pool.addTask([]{ return; });

    REQUIRE(res1.get() == 42);
    REQUIRE(res2.get() == 3);
    REQUIRE(res3.get() == 99);
    REQUIRE(res4.get() == a);
    REQUIRE(res5.get() == NonVoidType<>{});
}

TEST_CASE("测试异步(固定容量版本)") {
    ThreadPool pool;
    pool.run<ThreadPool::Model::FixedSizeAndNoCheck>(
        std::chrono::milliseconds {1000}, ThreadPoolDefaultStrategy
    );
    pool.addTask([] { return 42; })
    .thenTry([](Try<int> x) -> std::string {
        if (x) {
            log::hxLog.info(x.get());
            CHECK(true);
        }
        return "123";
    }).thenTry([](Try<std::string> x) {
        if (x) {
            log::hxLog.info("str:", x.move());
            CHECK(true);
        }
        throw std::runtime_error{"no err..."};
    }).thenTry([](Try<> x){
        if (x) {
            log::hxLog.info("void");
        } else {
            log::hxLog.error("err!", x.what());
            CHECK(true);
        }
        return x; // 继续传递异常
    }).thenTry([](Try<> x) {
        if (x) {
            log::hxLog.info("void");
        } else {
            log::hxLog.error("err!", x.what());
            CHECK(true);
        }
        // 如果没有, 就不传递了
    }).thenTry([](Try<> x) {
        if (x) [[likely]] {
            log::hxLog.info("这下不能传递了~");
            x.exception();
            CHECK(true);
        } else [[unlikely]] {
            log::hxLog.error("怎么还有问题???");
        }
    }).thenTry([](Try<> x) {
        if (x) {
            ;
        } else {
            log::hxLog.error("我一眼就看出来你有问题:", x.what());
            CHECK(true);
        }
    });
    log::hxLog.info("ok");

    // 类型测试
    Try<> {NonVoidType<>{}};
    Try<> {Uninitialized<void>{}.move()};
    Try<> {Uninitialized<void>{}.get()};
    Try<> {std::exception_ptr{}};

    // 阻塞等待
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}