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
                co_return co_await res.useRangeTransferFile("static/index.html");
            } catch (...) {
                co_return res.setStatusAndContent(
                    Status::CODE_500, "<h1>没有这个文件</h1>");
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
    // HX::web::server::ServerRun::startHttps("127.0.0.1", "28205", "certs/cert.pem", "certs/key.pem");
    HX::web::server::ServerRun::startHttp("127.0.0.1", "28205");
    return 0;
}

// 测试代码, 下面发现了一个潜在的问题
#include <HXSTL/coroutine/task/WhenAny.hpp>
#include <chrono>

HX::STL::coroutine::task::Task<bool> __text() {
    co_return false;
}

HX::STL::coroutine::task::Task<bool> _text() {
    // co_await HX::STL::coroutine::loop::TimerLoop::sleepFor(std::chrono::seconds{0});
    co_return co_await __text();
}

HX::STL::coroutine::task::Task<> test() {
    co_await HX::STL::coroutine::task::WhenAny::whenAny( // 如果下面参数互换, 则会段错误
        HX::STL::coroutine::loop::TimerLoop::sleepFor(std::chrono::seconds{3}),
        _text()
    );
}