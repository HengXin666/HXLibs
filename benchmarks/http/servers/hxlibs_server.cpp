#include <HXLibs/net/ApiMacro.hpp>

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "benchmark_payloads.hpp"

namespace {

std::size_t parsePositive(char const* value, char const* name) {
    std::size_t result{};
    auto const text = std::string_view{value};
    auto const [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (ec != std::errc{} || ptr != text.data() + text.size() || result == 0) {
        std::cerr << name << " must be a positive integer\n";
        std::exit(EXIT_FAILURE);
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    using namespace HX;
    using namespace HX::net;

    auto const port = argc > 1 ? parsePositive(argv[1], "port") : 18080;
    auto const workers = argc > 2 ? parsePositive(argv[2], "workers") : 1;
    auto const assetDir = std::string{argc > 3 ? argv[3] : "benchmarks/http/assets"};
    auto const htmlPath = assetDir + "/page.html";
    auto const payloadPath = assetDir + "/payload.bin";
    if (port > 65535) {
        std::cerr << "port must be at most 65535\n";
        return EXIT_FAILURE;
    }

    HttpServer server{static_cast<std::uint16_t>(port)};
    server.addEndpoint<GET>("/", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, benchmark_payloads::hello).sendRes();
    }).addEndpoint<GET>("/api/users", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, benchmark_payloads::json)
            .setContentType(JSON).sendRes();
    }).addEndpoint<GET>("/api/users/{userId}/orders/{orderId}", [] ENDPOINT {
        auto query = req.getParseQueryParameters();
        std::string body = "{\"user_id\":" + req.getPathParam(0).to<std::string>()
            + ",\"order_id\":" + req.getPathParam(1).to<std::string>()
            + ",\"page\":" + query["page"] + ",\"limit\":" + query["limit"]
            + ",\"sort\":\"" + query["sort"] + "\"}";
        co_await res.setStatusAndContent(Status::CODE_200, body).setContentType(JSON).sendRes();
    }).addEndpoint<GET>("/page.html", [htmlPath] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), htmlPath);
    }).addEndpoint<GET>("/payload.bin", [payloadPath] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), payloadPath);
    });
    server.syncRun(workers);
}
