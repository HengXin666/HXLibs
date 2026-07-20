#include <ylt/coro_http/coro_http_server.hpp>

#include <charconv>
#include <cstdlib>
#include <filesystem>
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
    using namespace cinatra;

    auto const port = argc > 1 ? parsePositive(argv[1], "port") : 18080;
    auto const workers = argc > 2 ? parsePositive(argv[2], "workers") : 1;
    auto const assetDir = std::filesystem::path{argc > 3 ? argv[3] : "benchmarks/http/assets"};
    if (port > 65535) {
        std::cerr << "port must be at most 65535\n";
        return EXIT_FAILURE;
    }

    coro_http_server server{workers, static_cast<unsigned short>(port)};
    server.set_http_handler<GET>("/", [](coro_http_request&, coro_http_response& response) {
        response.set_status_and_content(status_type::ok, std::string{benchmark_payloads::hello},
                                        content_encoding::none);
    });
    server.set_http_handler<GET>("/api/users", [](coro_http_request&, coro_http_response& response) {
        response.set_status_and_content(status_type::ok, std::string{benchmark_payloads::json},
                                        content_encoding::none);
    });
    server.set_http_handler<GET>("/api/users/:userId/orders/:orderId", [](
            coro_http_request& request, coro_http_response& response) {
        std::string body = "{\"user_id\":" + request.params_.at("userId")
            + ",\"order_id\":" + request.params_.at("orderId")
            + ",\"page\":" + std::string{request.get_query_value("page")}
            + ",\"limit\":" + std::string{request.get_query_value("limit")}
            + ",\"sort\":\"" + std::string{request.get_query_value("sort")} + "\"}";
        response.set_status_and_content(status_type::ok, std::move(body), content_encoding::none);
    });
    std::filesystem::current_path(assetDir.parent_path());
    server.set_static_res_dir("", assetDir.filename().string());
    server.sync_start();
}
