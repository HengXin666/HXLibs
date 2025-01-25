#include <iostream>
#include <filesystem>
#include <HXWeb/HXApiHelper.h>
#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXWeb/server/Server.h>

class HttpsController {
    ENDPOINT_BEGIN(API_GET, "/", root) {
        RESPONSE_DATA(
            200,
            "<h1>Hello This Heng_Xin 自主研发的 Https 文件服务器!</h1>",
            "text/html", "UTF-8"
        );
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/favicon.ico", faviconIco) {
        RESPONSE_DATA(
            200, 
            co_await HX::STL::utils::FileUtils::asyncGetFileContent("static/favicon.ico"),
            "image/x-icon"
        );
        co_return false;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/files/**", files) {
        // 使用分块编码
        RESPONSE_FILE(
            200,                 // 状态码
            "static/test/github.html",  // 分块读写的文件
            "text/html", "UTF-8" // 文件类型, 编码
        );
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/test", test) {
        RESPONSE_DATA(
            200, 
            co_await HX::STL::utils::FileUtils::asyncGetFileContent("static/test/github.html"),
            "text/html", "UTF-8"
        );
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/brack", break) { // 断开连接
        RESPONSE_DATA(
            200, 
            "ok",
            "text/html", "UTF-8"
        );
        co_return false;
    } ENDPOINT_END;
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