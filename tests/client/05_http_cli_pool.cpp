#include <HXLibs/net/client/HttpClientPool.hpp>
#include <HXLibs/net/ApiMacro.hpp>

using namespace HX;
using namespace net;
using namespace container;

int main() {
    HttpServer server{"127.0.0.1", "28205"};
    server.addEndpoint<GET>("/get", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    }).addEndpoint<POST>("/post", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    });
    std::size_t n = 1;
    server.asyncRun<decltype(utils::operator""_ms<"100">())>(1);
    HttpClientPool cliPool{n};
    for (std::size_t i = 0; i < 3; ++i) {
        cliPool.get("http://127.0.0.1:28205/get")
            .thenTry([&](
                auto t
            ) {
                if (!t) [[unlikely]] {
                    log::hxLog.error("err:", t.what());
                }
                log::hxLog.info("res =>", t.move());
                return cliPool.post(
                    "http://127.0.0.1:28205/post", "", HttpContentType::None
                ).thenTry([](container::Try<ResponseData> t) {
                    if (!t) [[unlikely]] {
                        log::hxLog.error("err:", t.what());
                    }
                    auto res = t.move();
                    log::hxLog.warning("res =>", res);
                    []{
                        [](int){
                            // 测试 throw
                        }([]() -> int {
                            throw std::runtime_error{"test"};
                        }());
                    }();
                    return 123456;
                })
                .thenTry([](container::Try<int> t) {
                    if (!t) [[unlikely]] {
                        log::hxLog.error("err:", t.what());
                        return -1;
                    }
                    log::hxLog.error("sb t = ", t.move());
                    return 999;
                });
            })
            // .thenTry([](auto t) {
            //     if (!t) [[unlikely]] {
            //         log::hxLog.error("err:", t.what());
            //     }
            //     log::hxLog.info("res =>", t.move().get());
            // });
            ;
    }

    std::this_thread::sleep_for(decltype(utils::operator""_ms<"200">())::StdChronoVal);
    return 0;
}