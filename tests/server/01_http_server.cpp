#include <HXLibs/net/Api.hpp>

using namespace HX;
using namespace net;
using namespace utils;

#include <iostream>

auto __init__ = []{
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        std::cout << "当前工作路径是: " << cwd << '\n';
        std::filesystem::current_path("../../../../");
        std::cout << "切换到路径: " << std::filesystem::current_path() << '\n';
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
    return 0;
}();

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

struct Test {
    auto before(Request&, Response&) {
        CHECK(true);
        return 1;
    }

    auto after(Request&, Response&) {
        CHECK(true);
        return true;
    }
};

TEST_CASE("测试普通请求") {
    HttpServer ser{"127.0.0.1", "28205",};
    ser.addEndpoint<GET, POST>("/", [](
        Request& req,
        Response& res
    ) -> coroutine::Task<> {
        (void)req;
        co_await res.setStatusAndContent(
            Status::CODE_200, "<h1>Hello HXLibs</h1>")
                    .sendRes();
        co_return;
    }, Test{});
    ser.asyncRun(1, 1500_ms); // 启动服务器
    std::this_thread::sleep_for((1500_ms).toChrono());
    {
        HttpClient cli;
        auto res = cli.get("http://127.0.0.1:28205/").get();
        CHECK(res.status == 200);
        CHECK(res.body == "<h1>Hello HXLibs</h1>");

        log::hxLog.info("status:", res.status);
        
        res = cli.get("/114514", {{"Connection", "close"}}).get();
        log::hxLog.info("status:", res.status);
        CHECK(res.status == 404);
    }
    {
        HttpClient cli;
        auto res = cli.post(
            "http://127.0.0.1:28205/", {{"Connection", "close"}},
            {}, HttpContentType::None
        ).get();
        CHECK(res.status == 200);
        CHECK(res.body == "<h1>Hello HXLibs</h1>");
    }
}