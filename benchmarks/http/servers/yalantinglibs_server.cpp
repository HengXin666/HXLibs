#include <ylt/coro_http/coro_http_server.hpp>

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string_view>

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
    if (port > 65535) {
        std::cerr << "port must be at most 65535\n";
        return EXIT_FAILURE;
    }

    coro_http_server server{workers, static_cast<unsigned short>(port)};
    server.set_http_handler<GET>("/", [](coro_http_request&, coro_http_response& response) {
        response.set_status_and_content(status_type::ok, "Hello World!",
                                        content_type::text_plain);
    });
    server.sync_start();
}
