#include <HXLibs/net/ApiMacro.hpp>
#include <HXLibs/net/client/HttpClientPool.hpp>

using namespace HX;
using namespace net;

auto hx_init = []{
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        log::hxLog.debug("当前工作路径是:", cwd);
        if (cwd.filename() == "build") { // cmake
            std::filesystem::current_path("../static");
        } else {
            std::filesystem::current_path("../../../../static");
        }
        log::hxLog.debug("切换到路径:", std::filesystem::current_path());
        log::hxLog.debug(std::this_thread::get_id());
    } catch (const std::filesystem::filesystem_error& e) {
        log::hxLog.error("Error:", e.what());
    }
    return 0;
}();


#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

TEST_CASE("一般性测试") {
    HttpServer server{28205};
    server.addEndpoint<POST>("/saveFile", [] ENDPOINT {
        log::hxLog.info("save File");
        co_await req.saveToFile("../build/saveFile.html");
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    });
    server.asyncRun(1);
    
    net::HttpClient cli{HttpClientOptions<decltype(utils::operator""_s<"60">())>{}};
    cli.uploadChunked<POST>("http://127.0.0.1:28205/saveFile", "./index.html")
        .thenTry([](container::Try<ResponseData> t) {
        if (!t) {
            log::hxLog.error(t.what());
        }
    }).wait();
    net::HttpClientPool cliPool{2};
    cliPool.uploadChunked<POST>("http://127.0.0.1:28205/saveFile", "./index.html")
        .wait();
    cliPool.uploadChunked<POST>("http://127.0.0.1:28205/saveFile", "./index.html")
        .wait();
}

TEST_CASE("debug") {
    HttpServer server{28205};
    server.addEndpoint<POST>("/saveFile", [] ENDPOINT {
        log::hxLog.info("save File");
        co_await req.saveToFile("../build/img.jpg");
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    }).addEndpoint<GET>("/getFile", [] ENDPOINT {
        log::hxLog.info("get File");
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "../build/img.jpg");
    });
    server.asyncRun(2);

    net::HttpClientPool cliPool{1};
    auto res = cliPool.uploadChunked<POST>("http://127.0.0.1:28205/saveFile", "./img/6.jpg");
    CHECK(res.get().get().body == "ok");
    CHECK(cliPool.uploadChunked<POST>("http://127.0.0.1:28205/saveFile", "./img/6.jpg").get().get().body == "ok");
}