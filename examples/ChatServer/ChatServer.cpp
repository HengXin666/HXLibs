#include <iostream>
#include <filesystem>
#include <HXWeb/HXApiHelper.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXWeb/server/Acceptor.h>
#include <HXJson/Json.h>
#include <HXSTL/coroutine/task/WhenAny.hpp>
#include <HXSTL/coroutine/loop/TriggerWaitLoop.h>
#include <HXWeb/server/Server.h>

using namespace std::chrono;

/**
 * @brief 实现一个轮询的聊天室 By Http服务器
 * 
 * 分析一下: 客户端会定时请求, 参数为当前消息的数量
 * 
 * 服务端如果 [客户端当前消息的数量 >= 服务端消息池.size()], 
 *      则会返回null
 * 服务端如果 [客户端当前消息的数量 < 服务端消息池.size()], 
 *      则会返回 消息: index: 服务端消息池.begin() + 客户端当前消息的数量 ~ .end()
 * 
 * 困难点, json解析与封装 (得实现json的反射先...), 那可不行! 反正就那几个, 直接手动得了..
 */

#include <HXJson/ReflectJson.hpp>

struct MsgArr {
    struct Message {
        std::string user;
        std::string content;

        REFLECT_CONSTRUCTOR_ALL(Message, user, content)
    };

    std::vector<MsgArr::Message> arr;

    REFLECT_CONSTRUCTOR_ALL(MsgArr, arr)
} msgArr {std::vector<MsgArr::Message> {MsgArr::Message{"系统", "欢迎来到聊天室!"}}};

struct MsgArrConst {
    std::span<const MsgArr::Message> arr;

    REFLECT(arr)
};

#include <HXJson/UnReflectJson.hpp>

HX::STL::coroutine::loop::TriggerWaitLoop waitLoop {};

class ChatController {
    ENDPOINT_BEGIN(API_GET, "/", root) {
        RESPONSE_DATA(
            200,
            co_await HX::STL::utils::FileUtils::asyncGetFileContent("indexByJson.html"),
            "text/html", "UTF-8"
        );
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/index", index) {
        RESPONSE_DATA(
            200,
            HX::STL::utils::DateTimeFormat::formatWithMilli(),
            "text/html", "UTF-8"
        );
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/end", end) {
        exit(0);
        co_return false;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/favicon.ico", faviconIco) {
        RESPONSE_DATA(
            200, 
            co_await HX::STL::utils::FileUtils::asyncGetFileContent("favicon.ico"),
            "image/x-icon"
        );
        co_return false;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_POST, "/send", send) { // 客户端发送消息过来
        auto body = io.getRequest().getRequesBody();
        auto jsonPair = HX::json::parse(body);
        if (jsonPair.second) {
            msgArr.arr.emplace_back(jsonPair.first);
            // printf("%s\n", Message::toJson(messageArr.begin(), messageArr.end()).c_str());
            waitLoop.runAllTask();
        } else {
            printf("解析客户端出错\n");
        }
        
        RESPONSE_STATUS(200).setContentType("text/plain", "UTF-8").setBodyData("OK");
        co_return true;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_POST, "/recv", recv) { // 发送内容给客户端
        using namespace std::chrono_literals;

        auto body = io.getRequest().getRequesBody();
        // printf("recv (%s)\n", body.c_str());
        auto jsonPair = HX::json::parse(body);

        if (jsonPair.second) {
            int len = jsonPair.first["first"].get<int>();
            // printf("内容是: %d\n", len);
            if (len < (int)msgArr.arr.size()) {
                // printf("立马回复, 是新消息~\n");
                RESPONSE_DATA(
                    200,
                    MsgArrConst(std::span<const MsgArr::Message> {
                        msgArr.arr.begin() + len, 
                        msgArr.arr.end()
                    }).toJson(),
                    "text/plain", "UTF-8"
                );
                co_return true;
            }
            else {
                // printf("等我3秒~\n");
                co_await HX::STL::coroutine::task::WhenAny::whenAny(
                    HX::STL::coroutine::loop::TriggerWaitLoop::triggerWait(waitLoop),
                    HX::STL::coroutine::loop::TimerLoop::sleepFor(3s)
                );

                RESPONSE_DATA(
                    200,
                    MsgArrConst(std::span<const MsgArr::Message> {
                        msgArr.arr.begin() + std::min(static_cast<long>(len), static_cast<long>(msgArr.arr.size())), 
                        msgArr.arr.end()
                    }).toJson(),
                    "text/plain", "UTF-8"
                );
                co_return true;
            }
        } else {
            printf("啥也没有...\n");
        }
        co_return true;
    } ENDPOINT_END;

public: // 控制器成员函数 (请写成`static`方法)

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
    ROUTER_BIND(ChatController);
    
    // 启动服务 (指定使用一个线程 (因为messageArr得同步, 多线程就需要上锁(我懒得写了qwq)))
    HX::web::server::ServerRun::startHttp("127.0.0.1", "28205", 1, 10s);
    return 0;
}