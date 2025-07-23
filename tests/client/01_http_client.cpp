#include <HXLibs/net/client/HttpClient.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;


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
    coMain().start();
    HttpClient cli{};
    auto res = cli.get("http://httpbin.org/get").get();
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
    cli.close();
    log::hxLog.debug("end");
    return 0;
}