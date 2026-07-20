#define ASIO_STANDALONE

#include <asio.hpp>

#include <array>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

#include "benchmark_payloads.hpp"

namespace {

std::string makeResponse(std::string_view contentType, std::string_view body) {
    return "HTTP/1.1 200 OK\r\nContent-Type: " + std::string{contentType}
        + "\r\nContent-Length: " + std::to_string(body.size())
        + "\r\nConnection: keep-alive\r\n\r\n" + std::string{body};
}

std::string const kHelloResponse = makeResponse("text/plain", benchmark_payloads::hello);
std::string const kJsonResponse = makeResponse("application/json", benchmark_payloads::json);
std::string const kHtmlResponse = makeResponse("text/html; charset=utf-8", benchmark_payloads::html);
std::string const kPayloadResponse = makeResponse("application/octet-stream", benchmark_payloads::payload64k);

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

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(asio::ip::tcp::socket socket) : socket_{std::move(socket)} {}

    void run() { read(); }

private:
    void read() {
        auto self = shared_from_this();
        asio::async_read_until(socket_, buffer_, "\r\n\r\n",
            [self](std::error_code ec, std::size_t bytes) {
                if (ec) {
                    return;
                }
                auto const begin = asio::buffers_begin(self->buffer_.data());
                std::string_view request{&*begin, bytes};
                if (request.starts_with("GET /api/users ")) {
                    self->response_ = &kJsonResponse;
                } else if (request.starts_with("GET /page.html ")) {
                    self->response_ = &kHtmlResponse;
                } else if (request.starts_with("GET /payload.bin ")) {
                    self->response_ = &kPayloadResponse;
                } else {
                    self->response_ = &kHelloResponse;
                }
                self->buffer_.consume(bytes);
                self->write();
            });
    }

    void write() {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(*response_),
            [self](std::error_code ec, std::size_t) {
                if (!ec) {
                    self->read();
                }
            });
    }

    asio::ip::tcp::socket socket_;
    asio::streambuf buffer_;
    std::string const* response_{&kHelloResponse};
};

class Server {
public:
    Server(asio::io_context& context, unsigned short port)
        : acceptor_{context} {
        asio::ip::tcp::endpoint endpoint{asio::ip::tcp::v4(), port};
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::socket_base::reuse_address{true});
        acceptor_.bind(endpoint);
        acceptor_.listen(asio::socket_base::max_listen_connections);
        accept();
    }

private:
    void accept() {
        acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket))->run();
            }
            accept();
        });
    }

    asio::ip::tcp::acceptor acceptor_;
};

} // namespace

int main(int argc, char** argv) {
    auto const port = argc > 1 ? parsePositive(argv[1], "port") : 18080;
    auto const workers = argc > 2 ? parsePositive(argv[2], "workers") : 1;
    if (port > 65535) {
        std::cerr << "port must be at most 65535\n";
        return EXIT_FAILURE;
    }

    asio::io_context context{static_cast<int>(workers)};
    Server server{context, static_cast<unsigned short>(port)};
    std::vector<std::thread> threads;
    threads.reserve(workers > 0 ? workers - 1 : 0);
    for (std::size_t index = 1; index < workers; ++index) {
        threads.emplace_back([&context] { context.run(); });
    }
    context.run();
    for (auto& thread : threads) {
        thread.join();
    }
}
