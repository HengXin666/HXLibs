#include <HXLibs/net/ApiMacro.hpp>

#include <charconv>
#include <cstdlib>
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

// 静态资源在启动时读入内存: 压测的是 HTTP 框架本身, 而非磁盘 IO;
// 与 nginx 的 open_file_cache/页缓存处于同一起跑线。
std::string readFileOrExit(std::string const& path) {
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
    using namespace HX;
    using namespace HX::net;

    auto const port = argc > 1 ? parsePositive(argv[1], "port") : 18080;
    auto const workers = argc > 2 ? parsePositive(argv[2], "workers") : 1;
    auto const assetDir = std::string{argc > 3 ? argv[3] : "benchmarks/http/assets"};
    auto const html = readFileOrExit(assetDir + "/page.html");
    auto const payload = readFileOrExit(assetDir + "/payload.bin");
    auto const htmlFile = assetDir + "/files/page-file.html";
    auto const payloadFile = assetDir + "/files/payload-file.bin";
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
    }).addEndpoint<GET>("/page.html", [html] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, html)
            .setContentType(HTML).sendRes();
    }).addEndpoint<GET>("/payload.bin", [payload] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, payload)
            .setContentType(HttpContentType::OctetStream).sendRes();
    }).addEndpoint<GET>("/page-file.html", [htmlFile] ENDPOINT {
        // 磁盘变体: 走框架自己的文件传输 API, 每请求真实读盘
        co_await res.useRangeTransferFile(req.getRangeRequestView(), htmlFile);
    }).addEndpoint<GET>("/payload-file.bin", [payloadFile] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), payloadFile);
    });
    server.syncRun(workers);
}
