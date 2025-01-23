#include <HXWeb/HXApiHelper.h>
#include <HXWeb/interceptor/RequestInterceptor.h>
#include <HXprint/print.h>

#include <chrono>

class ServerAddInterceptorTestController {
public:
    ENDPOINT_BEGIN(API_GET, "/", root) {
        RESPONSE_DATA(200, "good!", "text/html");
    } ENDPOINT_END;

    int __init___ = [] {
        using namespace HX::web::protocol::http;
        
        struct Log {
            decltype(std::chrono::steady_clock::now()) t;

            bool before(Request& req, Response& res) {
                HX::print::println("请求了: ", req.getPureRequesPath());
                static_cast<void>(res);
                t = std::chrono::steady_clock::now();
                return true;
            }

            bool after(Request& req, Response& res) {
                auto t1 = std::chrono::steady_clock::now();
                auto dt = t1 - t;
                int64_t us = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
                HX::print::println("已响应: ", req.getPureRequesPath(), "花费: ", us, " us");
                static_cast<void>(res);
                return true;
            }
        };

        HX::web::router::Router::getRouter().addEndpoint<GET, POST>("/", [](
            Request& req,
            Response& res
        ) -> HX::STL::coroutine::task::Task<> {
            auto map = req.getParseQueryParameters();
            if (map.find("loli") == map.end()) {
                res.setResponseLine(Response::Status::CODE_200)
                   .setContentType("text/html")
                   .setBodyData("<h1>You is no good!</h1>");
                co_return;
            }
            res.setResponseLine(Response::Status::CODE_200)
                .setContentType("text/html")
                .setBodyData("<h1>yo si yo si!</h1>");
        }, Log{});

        return 0;
    } ();
};

int main() {
    ROUTER_BIND(ServerAddInterceptorTestController);
    HX::web::server::ServerRun::startHttp("127.0.0.1", "28205");
    return 0;
}