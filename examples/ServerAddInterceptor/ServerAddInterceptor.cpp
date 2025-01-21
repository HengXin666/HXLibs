#include <HXWeb/HXApiHelper.h>
#include <HXWeb/interceptor/RequestInterceptor.h>

class ServerAddInterceptorTestController {
public:
    ENDPOINT_BEGIN(API_GET, "/", root) {
        RESPONSE_DATA(200, "good!", "text/html");
    } ENDPOINT_END;
};

int main() {
    ROUTER_BIND(ServerAddInterceptorTestController);
    HX::web::interceptor::RequestInterceptor::getRequestInterceptor()
        .addPreHandle([](HX::web::server::IO<>& io) -> HX::web::interceptor::RequestFlow {
        GET_PARSE_QUERY_PARAMETERS(urlData);
        if (urlData.find("loli") == urlData.end()) {
            RESPONSE_DATA(405, "no good!", "text/html");
            return HX::web::interceptor::RequestFlow::Block;
        }
        return HX::web::interceptor::RequestFlow::Pass;
    });
    HX::web::server::Server::startHttp("127.0.0.1", "28205");
    return 0;
}