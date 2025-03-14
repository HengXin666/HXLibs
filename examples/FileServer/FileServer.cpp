#include <iostream>
#include <filesystem>
#include <HXWeb/HXApi.hpp>
#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXWeb/server/Server.h>

class HttpsController {
    ROUTER
        .on<GET>("/", [] ENDPOINT {
            co_return res.setStatusAndContent(
                Status::CODE_200, "<h1>Hello This Heng_Xin 自主研发的 Https 文件服务器!</h1>");
        })
        .on<GET>("/favicon.ico", [] ENDPOINT {
            co_await res.useChunkedEncodingTransferFile("static/favicon.ico");
        })
        .on<GET>("/files/**", [] ENDPOINT {
            // 使用分块编码
            co_await res.useChunkedEncodingTransferFile("static/test/github.html");
        })
        .on<GET>("/test", [] ENDPOINT {
            // 不使用分块编码
            res.setStatusAndContent(
                Status::CODE_200, 
                co_await HX::STL::utils::FileUtils::asyncGetFileContent("static/test/github.html")
            );
        })
        .on<HEAD, GET>("/test/range", [] ENDPOINT {
            try {
                co_return co_await res.useRangeTransferFile("static/test/github.html");
            } catch (std::exception const& ec) {
                co_return res.setStatusAndContent(
                    Status::CODE_500, ec.what());
            }
        })
        .on<HEAD, GET>("/range/**", [] ENDPOINT {
            /*
                警告! 需要自己在 ./static 中创建bigFile文件夹, 因为github中并没有上传, 因为太大了
            */
            PARSE_MULTI_LEVEL_PARAM(path);
            try {
                co_return co_await res.useRangeTransferFile("static/bigFile/" + path);
            } catch (std::exception const& ec) {
                co_return res.setStatusAndContent(
                    Status::CODE_500, ec.what());
            }
        })
    ROUTER_END;
};


HX::STL::coroutine::task::Task<> test();

int main() {
    // return (void)test()._coroutine.resume(), 0;

    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        std::cout << "当前工作路径是: " << cwd << '\n';
        std::filesystem::current_path("../../../");
        std::cout << "切换到路径: " << std::filesystem::current_path() << '\n';
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
    ROUTER_BIND(HttpsController);
    // HX::STL::coroutine::task::runTask(HX::STL::coroutine::loop::AsyncLoop::getLoop(), test());
    // 启动服务
    HX::web::server::ServerRun::startHttps("127.0.0.1", "28205", "certs/cert.pem", "certs/key.pem");
    // HX::web::server::ServerRun::startHttp("127.0.0.1", "28205");
    return 0;
}