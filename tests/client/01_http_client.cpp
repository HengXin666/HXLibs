#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/net/Api.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;
using namespace container;

coroutine::Task<> coMain() {
    HttpClient cli{};
    log::hxLog.debug("开始请求");
    auto res = (co_await cli.coGet("http://0.0.0.0:28205/get")).move();
    log::hxLog.info("状态码:", res.status);
    log::hxLog.info("拿到了 头:", res.headers);
    log::hxLog.info("拿到了 体:", res.body);
    co_await cli.coClose();
}

int main() {
    HttpServer server{"0.0.0.0", "28205"};
    server.addEndpoint<GET>("/get", [] ENDPOINT {
        std::string json;
        reflection::toJson<true>(req.getHeaders(), json);
        auto& io = req.getIO();
        co_await res.setStatusAndContent(Status::CODE_400, std::move(json))
                    .sendRes();
        co_await io.close();
    });
    server.asyncRun<decltype(utils::operator""_s<"1">())>(1);

    coroutine::EventLoop loop;
    loop.sync(coMain());

    HttpClient cli{};
    auto t = cli.get("http://0.0.0.0:28205/get").get();
    do {
        if (!t) [[unlikely]] {
            log::hxLog.error("coMain:", t.what());
            break;
        }
        auto res = t.move();
        log::hxLog.info("状态码:", res.status);
        log::hxLog.info("拿到了 头:", res.headers);
        log::hxLog.info("拿到了 体:", res.body);
    } while (false);
    
    // 支持异步 API thenTry(Try<T>)
    cli.get("http://0.0.0.0:28205/get").thenTry([](Try<ResponseData> resTry) {
        if (resTry) {
            auto res = resTry.move();
            log::hxLog.info("状态码:", res.status);
            log::hxLog.info("拿到了 头:", res.headers);
            log::hxLog.info("拿到了 体:", res.body);
        } else {
            log::hxLog.error("发生异常:", resTry.what());
        }
        return resTry;
    });

    cli.close();
    log::hxLog.debug("end");
    return 0;
}