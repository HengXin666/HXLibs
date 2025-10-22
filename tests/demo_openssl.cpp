#include <HXLibs/net/Api.hpp>
#include <HXLibs/log/Log.hpp>

int main() {
    using namespace HX;
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        log::hxLog.debug("当前工作路径是:", cwd);
        if (cwd.filename() == "build") { // cmake
            std::filesystem::current_path("../");
        } else {
            std::filesystem::current_path("../../../");
        }
        log::hxLog.debug("切换到路径:", std::filesystem::current_path());
        log::hxLog.debug(std::this_thread::get_id());
    } catch (const std::filesystem::filesystem_error& e) {
        log::hxLog.error("Error:", e.what());
    }
    HX::net::HttpServer server{"127.0.0.1", "28205"};
    server.syncRun(1, []{
        HX::net::Context::getContext().initServerSSL({
            "certs/cert.pem",
            "certs/key.pem"
        });
    });
}