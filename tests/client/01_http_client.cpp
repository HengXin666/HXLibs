#include <HXLibs/net/client/HttpClient.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;
using namespace container;

coroutine::Task<> coMain() {
    HttpClient cli{};
    log::hxLog.debug("开始请求");
    ResponseData res = co_await cli.coGet("http://httpbin.org/get");
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
    co_await cli.coClose();
}

int main() {
    coMain().runSync();
    HttpClient cli{};
    auto res = cli.get("http://httpbin.org/get").get();
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
    
    // 支持异步 API thenTry(Try<T>)
    cli.get("http://httpbin.org/get").thenTry([](Try<ResponseData> resTry) {
        if (resTry) {
            auto res = resTry.move();
            log::hxLog.info("状态码:", res.status);
            log::hxLog.info("拿到了 头:", res.headers);
            log::hxLog.info("拿到了 体:", res.body);
        } else {
            log::hxLog.error("发生异常:", resTry.what());
        }
    });

    cli.close();
    log::hxLog.debug("end");
    return 0;
}