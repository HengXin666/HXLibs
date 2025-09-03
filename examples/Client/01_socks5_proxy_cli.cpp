#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/net/Api.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;
using namespace utils;

using namespace std::chrono;

int main() {
    log::hxLog.info("经过代理, 进行请求~");
    // 使用代理
    HttpClient cli{HttpClientOptions{{"socks5://127.0.0.1:2334"}}};
    auto res = cli.get("http://httpbin.org/get").get().move();
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
    return 0;
}