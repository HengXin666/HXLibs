#include <iostream>
#include <filesystem>
#include <HXWeb/HXApiHelper.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXWeb/protocol/websocket/WebSocket.h>
#include <HXWeb/server/Server.h>

using namespace std::chrono;

#include <HXSTL/coroutine/loop/IoUringLoop.h>

/**
 * @brief 实现一个websocket的聊天室 By Http服务器
 */

class WSChatController {
    ENDPOINT_BEGIN(API_GET, "/favicon.ico", faviconIco) {
        RESPONSE_DATA(
            200, 
            co_await HX::STL::utils::FileUtils::asyncGetFileContent("favicon.ico"),
            "image/x-icon"
        );
        co_return false;
    } ENDPOINT_END;



    ENDPOINT_BEGIN(API_GET, "/debug", debug) {
        RESPONSE_DATA(
            200, 
            co_await HX::STL::utils::FileUtils::asyncGetFileContent("favicon.ico"),
            "image/x-icon"
        );
#ifdef _DEBUG_MAP
        auto&& cntMap = HX::STL::coroutine::loop::debugMap.getMapCnt();
        printf("\033[0m\033[1;31m");
        for (auto&& [k, v] : cntMap)
            printf("%x %d\n", k, v);
        printf("\033[0m");
#endif
        std::cout << std::this_thread::get_id() << '\n';
        co_return false;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/home/{id}/{name}", getIdAndNameByHome) {
        START_PARSE_PATH_PARAMS; // 开始解析请求路径参数
        PARSE_PARAM(0, u_int32_t, id, false);
        PARSE_PARAM(1, std::string, name);

        // 解析查询参数为键值对; ?awa=xxx 这种
        GET_PARSE_QUERY_PARAMETERS(queryMap);

        if (queryMap.count("loli")) // 如果解析到 ?loli=xxx
            std::cout << queryMap["loli"] << '\n'; // xxx 的值

        RESPONSE_DATA(
            200, 
            "<h1> Home id 是 " 
            + std::to_string(*id) 
            + ", 而名字是 " 
            + *name 
            + "</h1><h2> 来自 URL: " 
            + io.getRequest().getRequesPath() 
            + " 的解析</h2>",
            "text/html", "UTF-8"
        );
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/files/**", files) { // http 模式
        // 使用分块编码
        RESPONSE_FILE(
            200,                 // 状态码
            "test/github.html",  // 分块读写的文件
            "text/html", "UTF-8" // 文件类型, 编码
        );
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/", root) {
        if (auto ws = co_await HX::web::protocol::websocket::WebSocket::makeServer(io)) {
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

            co_await ws->start();
            co_return false;
        } else {
            RESPONSE_DATA(
                200,
                co_await HX::STL::utils::FileUtils::asyncGetFileContent("WebSocketIndex.html"),
                "text/html", "UTF-8"
            );
            co_return true;
        }
    } ENDPOINT_END;
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
    ERROR_ENDPOINT_BEGIN { // 自定义 404 界面 (找不到url对应的端点函数时候展示的界面)
        RESPONSE_DATA(
            404,
            "<!DOCTYPE html><html><head><meta charset=UTF-8><title>404 Not Found</title><style>body{font-family:Arial,sans-serif;text-align:center;padding:50px;background-color:#f4f4f4}h1{font-size:100px;margin:0;color:#333}p{font-size:24px;color:red}</style><body><h1>404</h1><p>Not Found</p><hr/><p>HXNet</p>",
            "text/html", "UTF-8"
        );
        co_return false;
    } ERROR_ENDPOINT_END;

    // 启动服务
    HX::web::server::Server::startHttp("127.0.0.1", "28205", 16, 5s); 
    return 0;
}