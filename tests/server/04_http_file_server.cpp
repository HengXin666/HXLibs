#include <HXLibs/net/Api.hpp>

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

int main() {
    HttpServer server{"127.0.0.1", "28205"};
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
    
    log::hxLog.warning("===== end =====");
    return 0;
}