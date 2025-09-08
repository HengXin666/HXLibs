#include <HXLibs/net/client/HttpClientPool.hpp>
#include <HXLibs/net/Api.hpp>

using namespace HX;
using namespace net;

int main() {
    HttpServer server{"0.0.0.0", "28205"};
    server.addEndpoint<GET>("/get", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    });
    server.asyncRun(1);
    HttpClientPool cliPool{4};
    for (std::size_t i = 0; i < 4; ++i) {
        cliPool.get("http://0.0.0.0:28205/get")
            .thenTry([](auto t) {
                if (!t) [[unlikely]] {
                    log::hxLog.error("err:", t.what());
                }
                auto res = t.move();
                log::hxLog.info("res =>", res);
            });
    }
    return 0;
}