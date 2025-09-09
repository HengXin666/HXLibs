#include <HXLibs/net/client/HttpClientPool.hpp>
#include <HXLibs/net/Api.hpp>

using namespace HX;
using namespace net;

int main() {
    HttpServer server{"0.0.0.0", "28205"};
    server.addEndpoint<GET>("/get", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    }).addEndpoint<POST>("/post", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    });
    std::size_t n = 2;
    server.asyncRun(1);
    HttpClientPool cliPool{n};
    for (std::size_t i = 0; i < 1; ++i) {
        auto fr = cliPool.get("http://0.0.0.0:28205/get")
            .thenTry([&](auto t) {
                if (!t) [[unlikely]] {
                    log::hxLog.error("err:", t.what());
                }
                log::hxLog.info("res =>", t.move());
                return cliPool.post(
                    "http://0.0.0.0:28205/post", "", HttpContentType::None
                ).get();
            })
            .thenTry([](auto t) {
                if (!t) [[unlikely]] {
                    log::hxLog.error("err:", t.what());
                }
                auto res = t.move();
                log::hxLog.warning("res =>", res);
                return 123456;
            })
            .thenTry([](container::Try<int> t) {
                log::hxLog.error("sb t = ", t.move());
            });
    }

    std::this_thread::sleep_for(decltype(utils::operator""_ms<"200">())::StdChronoVal);
    return 0;
}