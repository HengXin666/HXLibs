#include <HXLibs/net/server/HttpServer.hpp>

using namespace HX::net;

int main() {
    HttpServer ser{"127.0.0.1", "28205"};
    ser.addEndpoint<GET>("/", [](
        Request& req,
        Response& res
    ) -> HX::coroutine::Task<> {
        (void)req;
        co_await res.setStatusAndContent(Status::CODE_200, "<h1>Hello</h1>")
                    .sendResponse();
        co_return;
    });
    // ser.sync(); // @todo cli
    return 0;
}