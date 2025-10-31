#include <HXLibs/net/ApiMacro.hpp>

HX_CONTROLLER(LoliController) {
    HX_ENDPOINT_MAIN([[maybe_unused]] int a, [[maybe_unused]] std::string const& b) {
        auto loliPtr = std::make_shared<int>();
    
        addEndpoint<GET>("/", [=] ENDPOINT {
            *loliPtr = 114514;
            co_return;
        });

        addEndpoint<POST>("/post", [=] ENDPOINT {
            *loliPtr = 2233;
            co_return;
        });
    }
};

#include <HXLibs/net/UnApiMacro.hpp>

int main() {
    using namespace HX::net;
    HttpServer server{"127.0.0.1", "28205"};

    addController<LoliController>(server, 1, "1");
}