#include <HXLibs/net/Api.hpp>

using namespace HX;

int main() {
    net::HttpServer server{"0.0.0.0", "28205"};
    server.addEndpoint("/{id}", [] ENDPOINT {
        log::hxLog.info("id =", req.getPathParam(0));
        log::hxLog.debug(req.getHeaders());
        co_await res.setStatusAndContent(net::Status::CODE_200, "ok")
                    .sendRes();
    });
    server.asyncRun(1);
    net::HttpClient cli;
    cli.post("http://0.0.0.0:28205/123", {}, "{}", 
            HX::net::HttpContentType::Json).wait();
    cli.post("http://0.0.0.0:28205/2233", {}, "{}", 
            HX::net::HttpContentType::Json).wait();
    return 0;
}