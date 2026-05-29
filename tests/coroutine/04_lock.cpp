#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/coroutine/awaiter/WhenAll.hpp>
#include <HXLibs/coroutine/task/RootTask.hpp>
#include <HXLibs/utils/TimeNTTP.hpp>
#include <HXLibs/log/Log.hpp>

#include <queue>

using namespace HX;

using namespace utils::time_nttp_literals;

class SerialScheduler {
    struct SerialAwaiter {
        explicit SerialAwaiter(SerialScheduler& mtx, coroutine::Task<> const& task)
            : _mtx{mtx}
            , _task{task}
        {}
        constexpr bool await_ready() const noexcept { return false; }
        constexpr bool await_suspend(std::coroutine_handle<> co) const noexcept {
            if (!_mtx._runCnt) {
                ++_mtx._runCnt;
                _mtx._q.push({_task, std::noop_coroutine()});
                return false;
            }
            _mtx._q.push({_task, co});
            return true;
        }
        constexpr void await_resume() const noexcept {}
    private:
        SerialScheduler& _mtx;
        coroutine::Task<> const& _task;
    };

#if 1
    coroutine::Task<> start() {
        auto [task, co] = std::move(_q.front());
        _q.pop();
        co_await task;
        co.resume();
        --_runCnt;
        if (_q.empty()) {
            co_return;
        }
        [this]() -> coroutine::RootTask<> {
            co_await start();
        }().detach();
    }
#else
    coroutine::Task<> start() {
        auto [task, co] = std::move(_q.front());
        co_await task;
        co.resume();
        _q.pop();
        if (_q.empty()) {
            co_return;
        }
        [this]() -> coroutine::RootTask<> {
            co_await start();
        }().detach();
    }
#endif
public:
    coroutine::Task<> serial(coroutine::Task<> const& t) {
        co_await SerialAwaiter{*this, t};
        if (_runCnt == 1) {
            log::hxLog.warning("mi mi mi");
            co_await start();
        }
    }
private:
    // 传递引用, 防止其因为析构引发的 段错误
    std::queue<std::tuple<coroutine::Task<> const&, std::coroutine_handle<>>> _q;
    std::size_t _runCnt{};
};

struct Test {
    void run() {
        auto t = loop.trySync(mainTask());
        if (!t) [[unlikely]] {
            log::hxLog.error(t.what());
        }
    }
private:
    template <std::size_t Id>
    coroutine::Task<> task([[maybe_unused]] SerialScheduler& mtx) {
        struct Raii {
            ~Raii() {
                log::hxLog.warning(Id, ": ~Raii()");
            }
        } _{};
        log::hxLog.info(std::format("{}: {}", Id, "begin {").c_str());
        co_await mtx.serial([&]() -> coroutine::Task<> {
            co_await loop.makeTimer().sleepFor((250_ms).toChrono());
            log::hxLog.info(std::format("{}: {}", Id, "ok <1>").c_str());
            co_await loop.makeTimer().sleepFor((500_ms).toChrono());
            log::hxLog.info(std::format("{}: {}", Id, "ok <2>").c_str());
            co_await loop.makeTimer().sleepFor((500_ms).toChrono());
            log::hxLog.info(std::format("{}: {}", Id, "ok <3>").c_str());
        }());
        co_await loop.makeTimer().sleepFor((800_ms).toChrono());
        log::hxLog.info(std::format("{}: {}", Id, "} // end").c_str());
    }

    coroutine::Task<> mainTask() {
        SerialScheduler mtx;
        co_await ([&]() -> coroutine::Task<> {
            co_await task<1>(mtx);
        }() || [&]() -> coroutine::Task<> {
            co_await task<2>(mtx);
        }() || [&]() -> coroutine::Task<> {
            co_await task<3>(mtx);
        }());
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
        SerialScheduler mtx;
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
        }() || [&]() -> coroutine::Task<> {
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

int main() {
    Test{}.run();
    TestIf1UseAfterFree{}.run();
}
