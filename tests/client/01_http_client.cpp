#include <HXLibs/net/client/HttpClient.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

template <typename Timeout>
coroutine::Task<> coMain(HttpClient<Timeout>& cli) {
    log::hxLog.debug("开始请求");
    ResponseData res = co_await cli.get("http://httpbin.org/get");
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
}

int main() {
    HttpClient cli{};
    cli.run(coMain(cli));
    log::hxLog.debug("end");
    return 0;
}