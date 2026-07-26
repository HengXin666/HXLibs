#include <ylt/coro_http/coro_http_server.hpp>

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

// 静态资源在启动时读入内存, 用 set_status_and_content_view 零拷贝发送,
// 避免 set_static_res_dir 每请求的文件系统开销。
std::string readFileOrExit(std::filesystem::path const& path) {
    std::ifstream file{path, std::ios::binary};
    std::string data{std::istreambuf_iterator<char>{file}, {}};
    if (!file || data.empty()) {
        std::cerr << "cannot read " << path << '\n';
        std::exit(EXIT_FAILURE);
    }
    return data;
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
    static std::string const html = readFileOrExit(assetDir / "page.html");
    static std::string const payload = readFileOrExit(assetDir / "payload.bin");

    coro_http_server server{workers, static_cast<unsigned short>(port)};
    server.set_http_handler<GET>("/", [](coro_http_request&, coro_http_response& response) {
        response.set_status_and_content_view(status_type::ok, benchmark_payloads::hello);
    });
    server.set_http_handler<GET>("/api/users", [](coro_http_request&, coro_http_response& response) {
        response.add_header("Content-Type", "application/json");
        response.set_status_and_content_view(status_type::ok, benchmark_payloads::json);
    });
    server.set_http_handler<GET>("/api/users/:userId/orders/:orderId", [](
            coro_http_request& request, coro_http_response& response) {
        std::string body = "{\"user_id\":" + request.params_.at("userId")
            + ",\"order_id\":" + request.params_.at("orderId")
            + ",\"page\":" + std::string{request.get_query_value("page")}
            + ",\"limit\":" + std::string{request.get_query_value("limit")}
            + ",\"sort\":\"" + std::string{request.get_query_value("sort")} + "\"}";
        response.add_header("Content-Type", "application/json");
        response.set_status_and_content(status_type::ok, std::move(body), content_encoding::none);
    });
    server.set_http_handler<GET>("/page.html", [](coro_http_request&, coro_http_response& response) {
        response.add_header("Content-Type", "text/html; charset=utf-8");
        response.set_status_and_content_view(status_type::ok, html);
    });
    server.set_http_handler<GET>("/payload.bin", [](coro_http_request&, coro_http_response& response) {
        response.add_header("Content-Type", "application/octet-stream");
        response.set_status_and_content_view(status_type::ok, payload);
    });
    // 磁盘变体: 用 cinatra 自带的静态资源目录 (每请求 coro_file 异步读盘),
    // files/ 内是 page-file.html / payload-file.bin, 挂载后路由为 /page-file.html 等
    std::filesystem::current_path(assetDir);
    server.set_static_res_dir("", "files");
    server.sync_start();
}
