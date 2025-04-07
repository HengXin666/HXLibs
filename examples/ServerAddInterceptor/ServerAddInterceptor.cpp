#include <HXWeb/HXApi.hpp>
#include <HXprint/print.h>

#include <chrono>

class ServerAddInterceptorTestController {
public:
    struct Log {
        decltype(std::chrono::steady_clock::now()) t;

        bool before(HX::web::protocol::http::Request& req, HX::web::protocol::http::Response& res) {
            HX::print::println("请求了: ", req.getPureRequesPath());
            static_cast<void>(res);
            t = std::chrono::steady_clock::now();
            return true;
        }

        bool after(HX::web::protocol::http::Request& req, HX::web::protocol::http::Response& res) {
            auto t1 = std::chrono::steady_clock::now();
            auto dt = t1 - t;
            int64_t us = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
            HX::print::println("已响应: ", req.getPureRequesPath(), "花费: ", us, " us");
            static_cast<void>(res);
            return true;
        }
    };

    ROUTER
        .on<GET, POST>("/", [](
            Request& req,
            Response& res
        ) -> Task<> {
            auto map = req.getParseQueryParameters();
            if (map.find("loli") == map.end()) {
                res.setResponseLine(Status::CODE_200)
                .setContentType(HX::web::protocol::http::ResContentType::html)
                .setBodyData("<h1>You is no good!</h1>");
                co_return;
            }
            res.setResponseLine(Status::CODE_200)
            .setContentType(HX::web::protocol::http::ResContentType::html)
            .setBodyData("<h1>yo si yo si!</h1>");
        }, Log{})
        .on<GET, POST>("/home/{id}/**", [](
            Request& req,
            Response& res
        ) -> Task<> {
            static_cast<void>(req);
            res.setResponseLine(Status::CODE_200)
            .setContentType(HX::web::protocol::http::ResContentType::html)
            .setBodyData("<h1>This is Home</h1>");
            co_return;
        })
    ROUTER_END;
};

int main() {
    ROUTER_BIND(ServerAddInterceptorTestController);
    HX::web::server::ServerRun::startHttp("127.0.0.1", "28205");
    return 0;
}