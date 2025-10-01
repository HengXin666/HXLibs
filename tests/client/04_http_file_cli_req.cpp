#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/net/Api.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;
using namespace utils;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

HttpServer server{"127.0.0.1", "28205"};

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
    server.addEndpoint<GET>("/", [] ENDPOINT {
        std::string json;
        reflection::toJson(req.getHeaders(), json);
        co_await res.setStatusAndContent(Status::CODE_200, json)
                    .sendRes();
    });
    server.addEndpoint<GET>("/img", [] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "./img/6.jpg");
    });
    server.asyncRun(1);
    return 0;
}();

TEST_CASE("测试客户端普通请求") {
    HttpClient cli;
    auto t = cli.get("http://127.0.0.1:28205/").get().move();
    CHECK(t.status == 200);
    log::hxLog.info(t.headers, t.body);
    CHECK(t.headers["content-length"].size() >= 3);
    CHECK(t.headers["content-type"] == "text/html; charset=UTF-8");
    CHECK(t.headers["connection"] == "keep-alive");
    CHECK(t.headers["server"] == "HXLibs::net");
    CHECK(t.body.size() == std::stoul(t.headers["content-length"]));
}

TEST_CASE("测试客户端文件下载(朴素)") {
    HttpClient cli;
    auto t = cli.get("http://127.0.0.1:28205/img").get().move();
    CHECK(t.status == 200);
    CHECK(t.body.size() == utils::FileUtils::getFileSize("./img/6.jpg"));
}