#include <HXLibs/net/ApiMacro.hpp>

using namespace HX;
using namespace net;
using namespace utils;

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

struct CoTest {
    coroutine::Task<int> before(Request&, Response&) {
        CHECK(true);
        co_return 1;
    }

    coroutine::Task<bool> after(Request&, Response&) {
        CHECK(true);
        co_return true;
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
    }, Test{}, CoTest{})
        .addEndpoint<GET>("/savefile", [] ENDPOINT {
            // 端点中读写文件的示例
            using namespace std::string_view_literals;
            utils::AsyncFile file{res.getIO()};
            co_await file.open("savefile.txt", OpenMode::Write);
            {
                std::vector<char> buf;
                for (char c : "可以协程的读写文件啦!!!"sv)
                    buf.push_back(c);
                co_await file.write(buf);
                co_await file.close();
            }
            co_await file.open("savefile.txt", OpenMode::Read);
            co_await res.setStatusAndContent(
                Status::CODE_200, co_await file.readAll())
                    .sendRes();
            co_await file.close();
        });
    ser.asyncRun(1, []{}, 1500_ms); // 启动服务器
    std::this_thread::sleep_for((1500_ms).toChrono());
    {
        HttpClient cli;
        auto res = cli.get("http://127.0.0.1:28205/").get().move();
        CHECK(res.status == 200);
        CHECK(res.body == "<h1>Hello HXLibs</h1>");

        log::hxLog.info("status:", res.status);
        
        res = cli.get("/114514", {{"Connection", "close"}}).get().move();
        log::hxLog.info("status:", res.status);
        CHECK(res.status == 404);
    }
    {
        HttpClient cli;
        auto res = cli.post(
            "http://127.0.0.1:28205/",
            {}, HttpContentType::None, {{"Connection", "close"}}
        ).get().move();
        CHECK(res.status == 200);
        CHECK(res.body == "<h1>Hello HXLibs</h1>");
    }
    {
        HttpClient cli;
        cli.get("http://127.0.0.1:28205/savefile")
            .thenTry([](container::Try<ResponseData> t) {
            if (!t) [[unlikely]] {
                CHECK(false);
                return;
            }
            auto res = t.move();
            CHECK(res.status == 200);
            CHECK(res.body == "可以协程的读写文件啦!!!");
        });
    }
}