#include <HXLibs/net/ApiMacro.hpp>

using namespace HX;
using namespace net;

enum class ServerStatus : uint32_t {
    Error = 0,
    Ok = 1
};

using Request = HttpRequest<HttpIO>;
using Response = HttpResponse<HttpIO>;

struct TimeLog {
    decltype(std::chrono::steady_clock::now()) t;

    
    auto before(Request&, Response&) {
        t = std::chrono::steady_clock::now();
        return ServerStatus::Ok;
    }

    auto after(Request& req, Response&) {
        auto t1 = std::chrono::steady_clock::now();
        auto dt = t1 - t;
        int64_t us = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
        log::hxLog.info("已响应: ", req.getPureReqPath(), "花费: ", us, " us");
        return true;
    }
};

#include <iostream>

auto hx_init = []{
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

int main() {
    HttpServer ser{28205};
    ser.addEndpoint<GET>("/", [](
        Request& req,
        Response& res
    ) -> coroutine::Task<> {
        (void)req;
        co_await res.setStatusAndContent(
            Status::CODE_200, "<h1>这是 HXLibs::net 服务器</h1>" + utils::DateTimeFormat::formatWithMilli())
                    .sendRes();
        co_return;
    }, TimeLog{}) // <-- 面向切面编程
    .addEndpoint<GET>("/stop", [&] ENDPOINT {
        co_await res.setStatusAndContent(
            Status::CODE_200, "<h1>stop server!</h1>" + utils::DateTimeFormat::formatWithMilli())
                    .sendRes();
        req.addHeaders("Connection", "close");
        ser.asyncStop();
        co_return;
    })
    .addEndpoint<GET>("/favicon.ico", [] ENDPOINT {
        log::hxLog.debug("get .ico");
        co_return co_await res.useRangeTransferFile(
            req.getRangeRequestView(),
            "static/favicon.ico"
        );
    })
    .addEndpoint<GET, HEAD>("/files/**", [] ENDPOINT {
        using namespace std::string_literals;
        using namespace std::string_view_literals;
        /*
            警告! 需要自己在 ./static 中创建bigFile文件夹, 因为github中并没有上传, 因为太大了
        */
        auto path = req.getUniversalWildcardPath();
        // log::hxLog.debug("请求:", path, "| 断点续传:", req.getReqType() == "HEAD"sv
        //     || req.getHeaders().contains("range"));
        bool isError = false;
        try {
            co_await res.useRangeTransferFile(
                req.getRangeRequestView(),
                "static/bigFile/"s + std::string{path.data(), path.size()}
            );
            log::hxLog.debug("发送完备!");
        } catch (std::exception const& ec) {
            log::hxLog.error(__FILE__, __LINE__, ":", ec.what());
            isError = true;
        }
        if (isError) [[unlikely]] {
            co_await res.setStatusAndContent(
                            Status::CODE_404,
                            "<h1>路径错误, 文件不存在<h1/>")
                        .sendRes();
        }
        co_return ;
    });
    ser.syncRun(2); // 启动服务器, 并且阻塞
}