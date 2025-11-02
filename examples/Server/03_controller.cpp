#include <HXLibs/net/ApiMacro.hpp>

struct LoliDAO {
    uint64_t select(uint64_t id) {
        return id;
    }
};

HX_CONTROLLER(LoliController) {
    HX_ENDPOINT_TEMPLATE_HEAD

    template <typename T>
    HX_ENDPOINT_TEMPLATE_MAIN(std::shared_ptr<T> loliDAO) {
        using namespace HX::net;
        addEndpoint<GET>("/", [=] ENDPOINT {
            auto id = loliDAO->select(114514);
            co_await res.setStatusAndContent(Status::CODE_200, std::to_string(id))
                        .sendRes();
        });
        addEndpoint<POST>("/post", [=] ENDPOINT {
            auto id = loliDAO->select(2233);
            co_await res.setStatusAndContent(Status::CODE_200, std::to_string(id))
                        .sendRes();
        });
    }
};

#include <HXLibs/net/UnApiMacro.hpp>

int main() {
    using namespace HX::net;
    HttpServer server{28205};

    // 依赖注入
    server.addController<LoliController>(std::make_shared<LoliDAO>());

    server.syncRun(1);
}