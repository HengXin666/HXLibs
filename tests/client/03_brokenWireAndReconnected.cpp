#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/net/Api.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;

int main() {
    setlocale(LC_ALL, "zh_CN.UTF-8");
    HttpServer server{"0.0.0.0", "28205"};
    using namespace utils;
    server.addEndpoint<GET>("/", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    });
    server.asyncRun(4, 1_s); // 设置 1s 自动断开连接
    HttpClient client;
    log::hxLog.info("连接ing...");
    client.connect("http://0.0.0.0:28205/");
    log::hxLog.info("连接了~~~");
    std::this_thread::sleep_for(decltype(2_s)::Val); // 等待 2s
    // 此时客户端已经被服务端断线了
    log::hxLog.info("连接ing...");
    client.connect("http://0.0.0.0:28205/");
    log::hxLog.info("连接了~~~");
    return 0;
}