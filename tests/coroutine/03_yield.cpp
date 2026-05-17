#include <HXLibs/coroutine/loop/EventLoop.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

coroutine::Task<int> range(int begin, int end = 0x3FFF'FFFF, int cnt = 1) {
    for (; begin < end; begin += cnt) {
        co_yield begin;
    }
    co_return end;
}

int main() {
    coroutine::EventLoop loop;
    loop.sync([]() -> coroutine::Task<> {
        auto r = range(10, 20);
        for (int i = 0; i < 10; ++i) {
            log::hxLog.info(co_await r);
        }
        co_return ;
    }());
}