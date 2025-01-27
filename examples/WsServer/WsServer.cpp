#include <iostream>
#include <filesystem>
#include <HXWeb/HXApi.hpp>
#include <HXSTL/utils/FileUtils.h>
#include <HXWeb/protocol/websocket/WebSocket.h>
#include <HXWeb/server/Server.h>

using namespace std::chrono;

#include <HXSTL/coroutine/loop/IoUringLoop.h>

/**
 * @brief 实现一个websocket的聊天室 By Http服务器
 */

class WSChatController {
    ROUTER
        .on<GET>("/home/{id}/{name}/**", [] ENDPOINT {
            PARSE_PARAM(0, u_int32_t, id);
            PARSE_PARAM(1, std::string, name);
            PARSE_MULTI_LEVEL_PARAM(uwp);
            // 解析查询参数为键值对; ?awa=xxx 这种
            GET_PARSE_QUERY_PARAMETERS(queryMap);

            if (queryMap.count("loli")) // 如果解析到 ?loli=xxx
                std::cout << queryMap["loli"] << '\n'; // xxx 的值

            RESPONSE_DATA(
                200,
                html,
                "<h1> Home id 是 " + std::to_string(*id) + ", "
                "而名字是 " + *name + "</h1>"
                "<h2> 来自 URL: " + req.getRequesPath() + " 的解析</h2>"
                "<h3>[通配符]参数为: " + uwp + "</h3>"
            );
        })
        .on<GET>("/home/{id}", [] ENDPOINT {
            PARSE_PARAM(0, u_int32_t, id);
            RESPONSE_DATA(
                200,
                html,
                "<h1> Home id 是 " + std::to_string(*id) + ", "
                "<h2> 来自 URL: " + req.getRequesPath() + " 的解析</h2>"
            );
        })
        .on<GET>("/files/**", [] ENDPOINT {
            PARSE_MULTI_LEVEL_PARAM(uwp);
            RESPONSE_DATA(
                200,
                html,
                "<h1> 访问file: " + uwp + "</h1>"
                "<h2> 来自 URL: " + req.getRequesPath() + " 的解析</h2>"
            );
            co_return;
        })
        .on<GET>("/", [] ENDPOINT {
            if (auto ws = co_await HX::web::protocol::websocket::WebSocket::makeServer(res.getIo())) {
                // 成功升级为 WebSocket
                printf("成功升级为 WebSocket\n");
                ws->setOnMessage([&](const std::string& message) -> HX::STL::coroutine::task::Task<> {
                    printf("收到消息: %s (%lu)\n", message.c_str(), message.size());
                    co_await ws->send("收到啦！" + message);
                });

                ws->setOnPong([&](std::chrono::steady_clock::duration dt) -> HX::STL::coroutine::task::Task<> {
                    printf("网络延迟: %ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(dt).count());
                    co_return;
                });

                ws->setOnClose([&]() -> HX::STL::coroutine::task::Task<> {
                    printf("正在关闭连接...\n");
                    co_return;
                });

                co_return co_await ws->start();
            } else {
                co_return co_await res.useChunkedEncodingTransferFile("WebSocketIndex.html");
            }
        })
        ROUTER_END;
};

int main() {
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        std::cout << "当前工作路径是: " << cwd << '\n';
        std::filesystem::current_path("../../../static");
        std::cout << "切换到路径: " << std::filesystem::current_path() << '\n';
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
    ROUTER_BIND(WSChatController);

    // 启动服务
    HX::web::server::ServerRun::startHttp("127.0.0.1", "28205", 16, 5s); 
    return 0;
}