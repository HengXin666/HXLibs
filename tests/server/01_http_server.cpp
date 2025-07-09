#include <HXLibs/net/server/HttpServer.hpp>

using namespace HX;
using namespace net;

struct TimeLog {
    decltype(std::chrono::steady_clock::now()) t;

    auto before(Request&, Response&) {
        t = std::chrono::steady_clock::now();
        return 1;
    }

    auto after(Request& req, Response&) {
        auto t1 = std::chrono::steady_clock::now();
        auto dt = t1 - t;
        int64_t us = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
        log::hxLog.info("已响应: ", req.getPureRequesPath(), "花费: ", us, " us");
        return true;
    }
};

int main() {
    HttpServer ser{"127.0.0.1", "28205"};
    ser.addEndpoint<GET>("/", [&](
        Request& req,
        Response& res
    ) -> coroutine::Task<> {
        (void)req;
        co_await res.setStatusAndContent(
            Status::CODE_200, "<h1>Hello</h1>" + utils::DateTimeFormat::formatWithMilli())
                    .sendResponse();
        ser.stop();
        co_return;
    }, TimeLog{});
    // ser.sync(); // @todo cli
    return 0;
}