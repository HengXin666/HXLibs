#include <iostream>
#include <filesystem>
#include <HXWeb/HXApi.hpp>
#include <HXSTL/utils/FileUtils.h>
#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXWeb/server/Acceptor.h>
#include <HXJson/Json.h>
#include <HXSTL/coroutine/task/WhenAny.hpp>
#include <HXSTL/coroutine/loop/TriggerWaitLoop.h>
#include <HXWeb/server/Server.h>
#include <HXprint/print.h>

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
    struct LogPrint {
        void print() const {
            HX::print::println("欢迎光临~");
        }
    };

    ROUTER
        .on<GET>("/", [log = LogPrint{}] ENDPOINT {
            RESPONSE_DATA(
                200,
                html,
                co_await HX::STL::utils::FileUtils::asyncGetFileContent("indexByJson.html")
            );
            log.print();
        })
        .on<GET, POST>("/nowTime", [] ENDPOINT {
            RESPONSE_DATA(
                200,
                html,
                HX::STL::utils::DateTimeFormat::formatWithMilli()
            );
            co_return;
        })
        .on<GET>("/favicon.ico", [] ENDPOINT {
            co_await res.useChunkedEncodingTransferFile("favicon.ico");
        })
    ROUTER_END;

    ROUTER
        .on<POST>("/send", [] ENDPOINT { // 客户端发送消息过来
            auto body = req.getRequesBody();
            auto jsonPair = HX::json::parse(body);
            if (jsonPair.second) {
                msgArr.arr.emplace_back(jsonPair.first);
                waitLoop.runAllTask();
            } else {
                printf("解析客户端出错\n");
            }
            
            RESPONSE_STATUS(200).setContentType(HX::web::protocol::http::ResContentType::text).setBodyData("OK");
            co_return;
        })
        .on<POST>("/recv", [] ENDPOINT {
            using namespace std::chrono_literals;
            auto jsonPair = HX::json::parse(req.getRequesBody());

            if (jsonPair.second) { // 发送内容给客户端
                int len = jsonPair.first["first"].get<int>();
                if (len < (int)msgArr.arr.size()) {
                    RESPONSE_DATA(
                        200,
                        text,
                        MsgArrConst(std::span<const MsgArr::Message> {
                            msgArr.arr.begin() + len, 
                            msgArr.arr.end()
                        }).toJson()
                    );
                    co_return;
                }
                else {
                    co_await HX::STL::coroutine::task::WhenAny::whenAny(
                        HX::STL::coroutine::loop::TriggerWaitLoop::triggerWait(waitLoop),
                        HX::STL::coroutine::loop::TimerLoop::sleepFor(3s)
                    );

                    RESPONSE_DATA(
                        200,
                        text,
                        MsgArrConst(std::span<const MsgArr::Message> {
                            msgArr.arr.begin() + std::min(static_cast<long>(len), static_cast<long>(msgArr.arr.size())), 
                            msgArr.arr.end()
                        }).toJson()
                    );
                    co_return;
                }
            } else {
                RESPONSE_STATUS(500).setContentType(HX::web::protocol::http::ResContentType::text).setBodyData("No");
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
    ROUTER_BIND(ChatController);

    // 设置路由失败时候的端点
    ROUTER_ERROR_ENDPOINT([] ENDPOINT {
        static_cast<void>(req);
        res.setResponseLine(HX::web::protocol::http::Status::CODE_404)
           .setContentType(HX::web::protocol::http::ResContentType::html)
           .setBodyData("<!DOCTYPE html><html><head><meta charset=UTF-8><title>404 Not Found</title><style>body{font-family:Arial,sans-serif;text-align:center;padding:50px;background-color:#f4f4f4}h1{font-size:100px;margin:0;color:#990099}p{font-size:24px;color:gold}</style><body><h1>404</h1><p>Not Found</p><hr/><p>HXLibs</p>");
        co_return;
    });
    
    // 启动服务 (指定使用一个线程 (因为messageArr得同步, 多线程就需要上锁(我懒得写了qwq)))
    HX::web::server::ServerRun::startHttp("127.0.0.1", "28205", 1, 10s);
    return 0;
}