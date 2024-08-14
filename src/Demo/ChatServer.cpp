#include <unistd.h>

#include <HXWeb/HXApiHelper.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXWeb/server/Acceptor.h>
#include <HXWeb/server/context/EpollContext.h>
#include <HXJson/HXJson.h>
#include <chrono>

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

struct Message {
    std::string _user;
    std::string _content;

    explicit Message(
        std::string user, 
        std::string content
    ) : _user(user)
      , _content(content)
    {}

    static std::string toJson(
        std::vector<Message>::iterator beginIt, 
        std::vector<Message>::iterator endIt
    ) {
        std::string res = "[";
        if (beginIt != endIt) {
            for (; beginIt != endIt; ++beginIt)
                res += "{\"user\":\""+ beginIt->_user + "\",\"content\":\"" + beginIt->_content  +"\"},";
            res.pop_back();
        }
        res += "]";
        return res;
    }
};

std::vector<Message> messageArr;

HX::web::server::context::StopSource recv_timeout_stop = HX::web::server::context::StopSource::make();

class ChatController {

    ENDPOINT_BEGIN(API_GET, "/", root) {
        req._responsePtr->setResponseLine(HX::web::protocol::http::Response::Status::CODE_200)
            .setContentType("text/html", "UTF-8")
            .setBodyData(HX::STL::utils::FileUtils::getFileContent("index.html"));
        co_return;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/favicon.ico", faviconIco) {
        req._responsePtr
            ->setResponseLine(HX::web::protocol::http::Response::Status::CODE_200)
            .setContentType("image/x-icon")
            .setBodyData(HX::STL::utils::FileUtils::getFileContent("favicon.ico"));
        co_return;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_GET, "/home/**", home) {
        req._responsePtr
            ->setResponseLine(HX::web::protocol::http::Response::Status::CODE_200)
            .setContentType("text/html", "UTF-8")
            .setBodyData("<h1> Home URL is " + req.getRequesPath() + "</h1>");
        co_return;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_POST, "/send", send) { // 客户端发送消息过来
        auto body = req.getRequesBody();
        auto jsonPair = HX::Json::parse(body);
        if (jsonPair.second) {
            messageArr.emplace_back(
                jsonPair.first.get<HX::Json::JsonDict>()["user"].get<std::string>(),
                jsonPair.first.get<HX::Json::JsonDict>()["content"].get<std::string>()
            );
            recv_timeout_stop.doRequestStop();
            recv_timeout_stop = HX::web::server::context::StopSource::make();
            printf("%s\n", Message::toJson(messageArr.begin(), messageArr.end()).c_str());
        } else {
            printf("解析客户端出错\n");
        }
        
        req._responsePtr
            ->setResponseLine(HX::web::protocol::http::Response::Status::CODE_200)
            .setContentType("text/plain", "UTF-8");
        co_return;
    } ENDPOINT_END;

    ENDPOINT_BEGIN(API_POST, "/recv", recv) { // 发送内容给客户端
        using namespace std::chrono_literals;

        auto body = req.getRequesBody();
        printf("recv (%s)\n", body.c_str());
        auto jsonPair = HX::Json::parse(body);

        if (jsonPair.second) {
            int len = jsonPair.first.get<HX::Json::JsonDict>()["first"].get<int>();
            printf("内容是: %d\n", len);
            if (len < (int)messageArr.size()) {
                printf("立马回复, 是新消息~\n");
                req._responsePtr->setResponseLine(HX::web::protocol::http::Response::Status::CODE_200)
                    .setContentType("text/plain", "UTF-8")
                    .setBodyData(Message::toJson(messageArr.begin() + len, messageArr.end()));
                co_return;
            }
            else {
                printf("等我3秒~\n");
                co_await HX::STL::coroutine::loop::TimerLoop::sleep_for(3s);
                std::vector<Message> submessages;
                printf("3秒之期已到, 马上回复~\n");
                req._responsePtr
                    ->setResponseLine(HX::web::protocol::http::Response::Status::CODE_200)
                    .setContentType("text/plain", "UTF-8")
                    .setBodyData(Message::toJson(messageArr.begin() + len, messageArr.end()));
                co_return;
            }
        } else {
            printf("啥也没有...\n");
        }
        co_return;
    } ENDPOINT_END;

public:

};

HX::STL::coroutine::awaiter::Task<> startChatServer() {
    setlocale(LC_ALL, "zh_CN.UTF-8");
    messageArr.emplace_back("系统", "欢迎来到聊天室!");
    ROUTER_BIND(ChatController);
    try {
        auto ptr = HX::web::server::Acceptor::make();
        co_await ptr->start("0.0.0.0", "28205");
    } catch(const std::system_error &e) {
        std::cerr << e.what() << '\n';
    }
    co_return;
}

#include <chrono>

using namespace std::chrono;

HX::STL::coroutine::awaiter::TimerTask C() {
    std::cout << "\t\tC start\n";
    co_await HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().sleep_for(3s); // Simulate an asynchronous operation
    std::cout << "\t\tC continues\n";
}

HX::STL::coroutine::awaiter::TaskSuspend<
    void,
    HX::STL::coroutine::awaiter::Promise<void>,
    HX::STL::coroutine::awaiter::ExitAwaiter<void, HX::STL::coroutine::awaiter::Promise<void>>
> B() {
    std::cout << "\tB start\n";
    // auto task = C(); // 需要保证 task 未被销毁!
    HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().addTimer(
        std::chrono::system_clock::now(),
        nullptr,
        std::make_shared<HX::STL::coroutine::awaiter::TimerTask>(C())
    );
    std::cout << "\tB end\n";
    co_return;
}

HX::STL::coroutine::awaiter::Task<
    void,
    HX::STL::coroutine::awaiter::Promise<void>,
    HX::STL::coroutine::awaiter::ExitAwaiter<void, HX::STL::coroutine::awaiter::Promise<void>>
> A() {
    std::cout << "A start\n";
    co_await B();
    std::cout << "A end\n";
}

int main() {
    chdir("../static");
    HX::STL::coroutine::awaiter::run_task(
        HX::STL::coroutine::loop::AsyncLoop::getLoop(), 
        startChatServer()
    );
    return 0;
}