#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/coroutine/awaiter/WhenAll.hpp>
#include <HXLibs/coroutine/executor/SerialExecutor.hpp>
#include <HXLibs/utils/TimeNTTP.hpp>
#include <HXLibs/log/Log.hpp>


using namespace HX;
using namespace utils::time_nttp_literals;


struct Test {
    void run() {
        auto t = loop.trySync(mainTask());
        if (!t) [[unlikely]] {
            log::hxLog.error(t.what());
        }
    }
private:
    template <std::size_t Id>
    coroutine::Task<> task([[maybe_unused]] coroutine::SerialExecutor& mtx) {
        log::hxLog.info(std::format("{}: {}", Id, "begin {").c_str());
        try {
            co_await mtx.serial([&]() -> coroutine::Task<> {
                struct Raii {
                    Raii() {
                        log::hxLog.warning(Id, ": Raii()");
                    }
                    ~Raii() {
                        log::hxLog.warning(Id, ": ~Raii()");
                    }
                } _{};
                co_await loop.makeTimer().sleepFor((250_ms).toChrono());
                log::hxLog.info(std::format("{}: {}", Id, "ok <1>").c_str());
                co_await loop.makeTimer().sleepFor((500_ms).toChrono());
                log::hxLog.info(std::format("{}: {}", Id, "ok <2>").c_str());
                co_await loop.makeTimer().sleepFor((500_ms).toChrono());
                // throw std::runtime_error{"sb"};
                log::hxLog.info(std::format("{}: {}", Id, "ok <3>").c_str());
            }());
        } catch (std::exception const& e) {
            log::hxLog.error("err:", e.what());
        }
        co_await loop.makeTimer().sleepFor((800_ms).toChrono());
        log::hxLog.info(std::format("{}: {}", Id, "} // end").c_str());
    }

    coroutine::Task<> mainTask() {
        coroutine::SerialExecutor mtx{}; // 下面的析构退出了. 但是 mtx 的生命周期比下面长
                                         // 导致 queue 没有立即销毁, 导致范围野内存
        co_await ([&]() -> coroutine::Task<> {
            co_await task<1>(mtx);
            // throw std::runtime_error{"sb 2"};
        }() || [&]() -> coroutine::Task<> {
            co_await task<2>(mtx);
        }() || [&]() -> coroutine::Task<> {
            co_await task<3>(mtx);
        }());
        throw std::runtime_error{"sb 1"};
    }
    coroutine::EventLoop loop;
};

struct TestIf1UseAfterFree {
    void run() {
        auto t = loop.trySync(mainTask());
        if (!t) [[unlikely]] {
            log::hxLog.error(t.what());
        }
    }
private:
    coroutine::Task<> mainTask() {
        coroutine::SerialExecutor mtx{};
        struct Raii {
            int id{};
            ~Raii() {
                log::hxLog.warning(id, ": ~Raii()");
            }
        };
        co_await ([&]() -> coroutine::Task<> {
            co_await mtx.serial([&]() -> coroutine::Task<> {
                co_await loop.makeTimer().sleepFor((100_ms).toChrono());
                log::hxLog.info("保持活动分支: 串行部分完成"); // 1
            }());
            Raii _{1};
            log::hxLog.info("keep-alive分支: 在任何挂起时保持");
            co_await loop.makeTimer().sleepFor((5000_ms).toChrono()); // 2
        }() && [&]() -> coroutine::Task<> {
            co_await mtx.serial([&]() -> coroutine::Task<> {
                co_await loop.makeTimer().sleepFor((200_ms).toChrono()); // 3
                log::hxLog.info("优胜者分支: 串行部分已完成");
            }());
            Raii _{2};
            log::hxLog.info("获胜者分支: 在SerialScheduler::start::co.resume()内完成"); // 4, 此时 2 还在
        }());

        // end
    }

    coroutine::EventLoop loop;
};

struct Test2 {
    void run() {
        auto t = loop.trySync(mainTask());
        if (!t) [[unlikely]] {
            log::hxLog.error(t.what());
        }
    }
private:
    template <std::size_t Id, bool Next = true>
    coroutine::Task<> task2() {
        log::hxLog.info(Id);
        co_await loop.makeTimer().sleepFor((100_ms).toChrono());
        // if constexpr (Next) {
        //     co_await task2<Id + 100, false>();
        // }
    }

    coroutine::Task<> mainTask() {
        coroutine::SerialExecutor mtx{}; // 下面的析构退出了. 但是 mtx 的生命周期比下面长
                                         // 导致 queue 没有立即销毁, 导致范围野内存
        for (int i = 0; i < 1; ++i) {
            auto res = co_await (mtx.serial(task2<1>()) || mtx.serial(task2<2>()));
        }
        log::hxLog.warning("=== end ===");
    }
    coroutine::EventLoop loop;
};

int main() {
    Test{}.run();
    Test2{}.run();
    TestIf1UseAfterFree{}.run();
    // std::this_thread::sleep_for((5_s).toChrono());
}
