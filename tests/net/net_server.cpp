#include <source_location>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/net/ApiMacro.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;
using namespace utils;

auto hx_init = []{
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        log::hxLog.debug("当前工作路径是:", cwd);
        std::filesystem::current_path("../../../../");
        log::hxLog.debug("切换到路径:", std::filesystem::current_path());
    } catch (const std::filesystem::filesystem_error& e) {
        log::hxLog.error("Error:", e.what());
    }
    return 0;
}();

void sayErr(
    std::string_view msg,
    const std::source_location location = std::source_location::current()
) {
    log::hxLog.error(location.file_name(), '(', location.line(), "):", location.function_name(), ':', msg);
}

void initServerSsl() {
#ifdef HXLIBS_ENABLE_SSL
    net::SslContext::get().init<SslContext::SslType::Server>({
        SslVerifyOption::None,
        "certs/server.crt",
        "certs/server.key"
    });
#endif // !HXLIBS_ENABLE_SSL
}

auto makeClient() {
#ifdef HXLIBS_ENABLE_SSL
    return HttpsClient client{HttpClientOptions{
        // ProxyType<Socks5Proxy>{"socks5://127.0.0.1:2333"}
        // ProxyType<HttpProxy>{"http://127.0.0.1:2334"}
    }};
#endif // !HXLIBS_ENABLE_SSL
    return HttpClient {HttpClientOptions{
        // ProxyType<Socks5Proxy>{"socks5://127.0.0.1:2333"}
        // ProxyType<HttpProxy>{"http://127.0.0.1:2334"}
    }};
}

void initClient([[maybe_unused]] auto&& client) {
#ifdef HXLIBS_ENABLE_SSL
    std::move(client).initSsl({
        .verifyOption = SslVerifyOption::None
    });
#endif // !HXLIBS_ENABLE_SSL
}

constexpr std::string makeUrl(std::string url) {
#ifdef HXLIBS_ENABLE_SSL
    return "https://" + url;
#endif // !HXLIBS_ENABLE_SSL
    return "http://" + url;
}

constexpr std::string makeWsUrl(std::string url) {
#ifdef HXLIBS_ENABLE_SSL
    return "wss://" + url;
#endif // !HXLIBS_ENABLE_SSL
    return "ws://" + url;
}

HX_CONTROLLER(Test01Controller) {
    HX_ENDPOINT_MAIN() {
        addEndpoint<GET>("/", [=] ENDPOINT {
            co_await res.setStatusAndContent(Status::CODE_200, "Hello Test01")
                        .sendRes();
        });

        // 测试切面
        struct AopTest {
            auto before(Request&, Response&) {
                CHECK(true);
                return 1;
            }

            auto after(Request&, Response&) {
                CHECK(true);
                return true;
            }
        };

        // 测试注入切面
        addEndpoint<GET>("/aop/default", [=] ENDPOINT {
            co_await res.setStatusAndContent(Status::CODE_200, "aop: default")
                        .sendRes();
        }, AopTest{}, AopCoTest<false>{});

        // 可以拦截, 并且返回响应的切面
        addEndpoint<GET>("/aop/return", [=] ENDPOINT {
            co_await res.setStatusAndContent(Status::CODE_200, "aop: return")
                        .sendRes();
        }, AopCoTest<true>{}, AopTest{});
    }

    // 测试协程切面, 协程切面可以拦截并且响应.
    template <bool IsRes>
    struct AopCoTest {
        coroutine::Task<int> before(Request&, Response& res) {
            CHECK(true);
            if constexpr (IsRes) {
                CHECK(true);
                co_await res.setStatusAndContent(Status::CODE_200, "AopCoTest: before out!")
                            .sendRes();
                co_return 0;
            }
            co_return 1;
        }

        coroutine::Task<bool> after(Request&, Response&) {
            CHECK(true);
            co_return true;
        }
    };
};

TEST_CASE("Test01: 测试 GET 请求 / AOP") {
    HttpServer server{28205};
    server.addController<Test01Controller>();
    server.asyncRun(1, initServerSsl, 1500_ms);
    auto client = makeClient();
    initClient(client);
    {
        auto t = client.get(makeUrl("127.0.0.1:28205/")).get();
        if (!t) {
            sayErr(t.what());
            CHECK(false);
        } else {
            auto data = t.move();
            if (data.status != 200) {
                sayErr("status != 200");
                CHECK(false);
            } else if (data.body != "Hello Test01") {
                sayErr("body != Hello Test01");
                CHECK(false);
            }
        }
    }
    {
        auto t = client.get(makeUrl("127.0.0.1:28205/aop/default")).get();
        if (!t) {
            sayErr(t.what());
            CHECK(false);
        } else {
            auto data = t.move();
            if (data.status != 200) {
                sayErr("status != 200");
                CHECK(false);
            } else if (data.body != "aop: default") {
                sayErr("body != aop: default");
                CHECK(false);
            }
        }
    }
    {
        auto t = client.get(makeUrl("127.0.0.1:28205/aop/return")).get();
        if (!t) {
            sayErr(t.what());
            CHECK(false);
        } else {
            auto data = t.move();
            if (data.status != 200) {
                sayErr("status != 200");
                CHECK(false);
            } else if (data.body != "AopCoTest: before out!") {
                sayErr("body != AopCoTest: before out!");
                CHECK(false);
            }
        }
    }
}

HX_CONTROLLER(Test02Controller) {
    HX_ENDPOINT_MAIN() {
        // 测试写
        addEndpoint<WS>("/ws/ok", [] ENDPOINT {
            auto ws = co_await WebSocketFactory::accept(req, res);
            co_await ws.sendText("ws ok");
            co_await ws.recvText(); // 等待客户端 close
        });
        // 测试读
        addEndpoint<WS>("/ws/send", [] ENDPOINT {
            auto ws = co_await WebSocketFactory::accept(req, res);
            co_await ws.sendText(co_await ws.recvText());
            co_await ws.recvText(); // 等待客户端 close
        });
        // 测试升级失败 (即不进行升级)
        addEndpoint<WS>("/ws/upErr", [] ENDPOINT {
            co_await res.setStatusAndContent(Status::CODE_200, "Hello Loli")
                        .sendRes();
        });
        // 测试断开连接
        addEndpoint<WS>("/ws/close", [] ENDPOINT {
            auto ws = co_await WebSocketFactory::accept(req, res);
            co_await ws.close();
        });
    }
};

TEST_CASE("Test02: 测试 WebSocket") {
    HttpServer server{28205};
    server.addController<Test02Controller>();
    server.asyncRun(1, initServerSsl, 1500_ms);
    auto client = makeClient();
    initClient(client);
    {
        client.wsLoop(makeWsUrl("127.0.0.1:28205/ws/ok"), [] (WebSocketClient ws) -> coroutine::Task<> {
            CHECK(co_await ws.recvText() == "ws ok");
            co_await ws.close();
        }).wait();
    }
    {
        client.wsLoop(makeWsUrl("127.0.0.1:28205/ws/send"), [] (WebSocketClient ws) -> coroutine::Task<> {
            auto str = net::internal::randomBase64();
            co_await ws.sendText(str);
            CHECK(co_await ws.recvText() == str);
            co_await ws.close();
        }).wait();
    }
    {
        auto t = client.wsLoop(makeWsUrl("127.0.0.1:28205/ws/upErr"), [] (auto) -> coroutine::Task<> {
            co_return;
        }).get();
        if (!t) {
            CHECK(true);
            log::hxLog.error(t.what());
        } else {
            CHECK(false);
        }
    }
    {
        auto t = client.wsLoop(makeWsUrl("127.0.0.1:28205/ws/close"), [] (WebSocketClient ws) -> coroutine::Task<> {
            co_await ws.recvText();
            co_return;
        }).get();
        if (!t) {
            CHECK(true);
            log::hxLog.error(t.what());
        } else {
            CHECK(false);
        }
    }
}

#include <HXLibs/net/UnApiMacro.hpp>