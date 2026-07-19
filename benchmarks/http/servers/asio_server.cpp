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

namespace {

constexpr std::string_view kResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 12\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "Hello World!";

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
                self->buffer_.consume(bytes);
                self->write();
            });
    }

    void write() {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(kResponse),
            [self](std::error_code ec, std::size_t) {
                if (!ec) {
                    self->read();
                }
            });
    }

    asio::ip::tcp::socket socket_;
    asio::streambuf buffer_;
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
