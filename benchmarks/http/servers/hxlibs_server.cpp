#include <HXLibs/net/ApiMacro.hpp>

#include <charconv>
#include <cstdlib>
#include <iostream>
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
    }).addEndpoint<GET>("/page.html", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, benchmark_payloads::html)
            .setContentType(HTML).sendRes();
    }).addEndpoint<GET>("/payload.bin", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, benchmark_payloads::payload64k)
            .setContentType(HttpContentType::OctetStream).sendRes();
    });
    server.syncRun(workers);
}
