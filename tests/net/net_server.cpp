#include <list>
#include <source_location>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/net/ApiMacro.hpp>
#include <HXLibs/log/Log.hpp>

#include <HXLibs/coroutine/awaiter/WhenAll.hpp>

using namespace HX;
using namespace net;
using namespace utils;

auto hx_init = []{
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        log::hxLog.debug("当前工作路径是:", cwd);
        if (cwd.filename() == "build") { // cmake
            std::filesystem::current_path("../");
        } else {
            std::filesystem::current_path("../../../../");
        }
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

#ifdef NO_HXLIBS_ENABLE_SSL
using WebClinet = HttpsClient<decltype(utils::operator""_ms<"5000">()), NoneProxy>;
#else
using WebClinet = HttpClient<decltype(utils::operator""_ms<"5000">()), NoneProxy>;
#endif
WebClinet makeClient(std::shared_ptr<coroutine::EventLoop> loop = nullptr) {
    // MSVC 无法基于 三元运算符进行 RVO 优化...
    if (loop) {
        return WebClinet{HttpClientOptions{}, std::move(loop)};
    }
    return WebClinet{HttpClientOptions{}};
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
            using Request = HttpRequest<HttpIO>;
            using Response = HttpResponse<HttpIO>;

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
        using Request = HttpRequest<HttpIO>;
        using Response = HttpResponse<HttpIO>;

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
    server.asyncRun(1, initServerSsl, 1500_s);
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
                log::hxLog.error("body is:", data.body);
                CHECK(false);
            }
        }
    }
}

HX_CONTROLLER(Test02Controller) {
    HX_ENDPOINT_MAIN() {
        auto wsPool = std::make_shared<WSPool>();

        // 测试写
        addEndpoint<WS>("/ws/ok", [] ENDPOINT {
            auto ws = co_await WebSocketFactory{req, res}.accept();;
            co_await ws.sendText("ws ok");
            co_await ws.recvText(); // 等待客户端 close
        });
        // 测试读
        addEndpoint<WS>("/ws/recv", [] ENDPOINT {
            auto ws = co_await WebSocketFactory{req, res}.accept();;
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
            auto ws = co_await WebSocketFactory{req, res}.accept();;
            co_await ws.close();
        });
        // 测试多发 + 发送 Json
        addEndpoint<WS>("/ws/all/send", [=] ENDPOINT {
            co_await wsPool->addWsAndKeepAlive(
                co_await WebSocketFactory{req, res}.accept()
            );
        })
        .addEndpoint<POST>("/ws/addMsg", [=] ENDPOINT {
            Msg msg;
            std::string json = co_await req.parseBody();
            reflection::fromJson(msg, json); // 保证是正确的 json, 否则异常
            co_await res.setStatusAndContent(Status::CODE_200, "ok")
                        .sendRes();
            co_await wsPool->sendAll(json);
        });
    }
public:
    struct Msg {
        uint64_t id;
        std::string name;
        std::string msg;

        constexpr bool operator==(Msg const& that) const noexcept {
            return id == that.id 
                && name == that.name
                && msg == that.msg;
        }
    };
private:
    // WebSocket 消息池
    // 正确使用应该是 一个 ws 接口专门用来发送消息
    // 而发送数据则应该使用另一个接口, 也就是不能使用这个ws
    struct WSPool {
        coroutine::Task<> sendAll(std::string_view msg) {
            if (_wsPool.empty())
                co_return;
            // 一次生成数据
            auto pk = WebSocketFactory<HttpIO>::makePacketView(OpCode::Text, msg);
            for (auto& ws : _wsPool) {
                // 多次重发这个数据
                co_await ws.sendPacketView(pk);
            }
        }
        
        // 添加到池中, 并且保活
        coroutine::Task<> addWsAndKeepAlive(WebSocket<WebSocketModel::Server, HttpIO> ws) {
            auto it = _wsPool.emplace(_wsPool.end(), ws);
            try {
                while (true) {
                    // 仅维持心跳, 客户端不应该发送除了 ping 以外的任何内容
                    co_await ws.recvText();
                }
            } catch (...) {
                // ws连接已断开
            }
            // 注意线程安全啊! 这里是单线程, 仅示例, 故没有封装和上锁
            _wsPool.erase(it);
        }
    private:
        std::list<WebSocket<WebSocketModel::Server, HttpIO>> _wsPool{};
    };
};

TEST_CASE("Test02: 测试 WebSocket") {
    HttpServer server{28205};
    server.addController<Test02Controller>();
    server.asyncRun(1, initServerSsl, 1500_ms);
    auto client = makeClient();
    initClient(client);
    {
        client.wsLoop(
            makeWsUrl("127.0.0.1:28205/ws/ok"),
            [] (
#ifdef HXLIBS_ENABLE_SSL
            net::WSSClient
#else
            net::WSClient
#endif // !#ifdef HXLIBS_ENABLE_SSL
                ws) -> coroutine::Task<> {
                CHECK(co_await ws.recvText() == "ws ok");
                co_await ws.close();
            }
        ).wait();
    }
    {
        client.wsLoop(
            makeWsUrl("127.0.0.1:28205/ws/recv"),
            [] (
#ifdef HXLIBS_ENABLE_SSL
            net::WSSClient
#else
            net::WSClient
#endif // !#ifdef HXLIBS_ENABLE_SSL
                 ws) -> coroutine::Task<> {
                auto str = net::internal::randomBase64();
                co_await ws.sendText(str);
                CHECK(co_await ws.recvText() == str);
                co_await ws.close();
            }
        ).wait();
    }
    {
        auto t = client.wsLoop(
            makeWsUrl("127.0.0.1:28205/ws/upErr"),
            [] (auto) -> coroutine::Task<> {
                co_return;
            }
        ).get();
        if (!t) {
            CHECK(true);
            log::hxLog.error(t.what());
        } else {
            CHECK(false);
        }
    }
    {
        auto t = client.wsLoop(
            makeWsUrl("127.0.0.1:28205/ws/close"),
            [] (
#ifdef HXLIBS_ENABLE_SSL
            net::WSSClient
#else
            net::WSClient
#endif // !#ifdef HXLIBS_ENABLE_SSL
                 ws) -> coroutine::Task<> {
                co_await ws.recvText();
                co_return;
            }
        ).get();
        if (!t) {
            CHECK(true);
            log::hxLog.error(t.what());
        } else {
            CHECK(false);
        }
    }
    {
        // 测试群发
        auto loop = std::make_shared<coroutine::EventLoop>();
        auto res = loop->trySync([&]() -> coroutine::Task<> {
            auto client01 = makeClient(loop);
            auto client02 = makeClient(loop);
            auto client03 = makeClient(loop);
            std::size_t n = 5;
            Test02Controller::Msg recvMsg;
            // 1. cli01/02 连接到 ws 中
            // 2. cli03 发送, server 广播
            co_await (client01.coWsLoop(
                makeWsUrl("127.0.0.1:28205/ws/all/send"),
                [&](
#ifdef HXLIBS_ENABLE_SSL
            net::WSSClient
#else
            net::WSClient
#endif // !#ifdef HXLIBS_ENABLE_SSL
                    ws) -> coroutine::Task<> {
                    for (std::size_t i = 0; i < n; ++i) {
                        CHECK(co_await ws.recvJson<Test02Controller::Msg>() == recvMsg);
                    }
                    co_await ws.close();
                }) && client02.coWsLoop(
                makeWsUrl("127.0.0.1:28205/ws/all/send"),
                [&](
#ifdef HXLIBS_ENABLE_SSL
            net::WSSClient
#else
            net::WSClient
#endif // !#ifdef HXLIBS_ENABLE_SSL
                    ws) -> coroutine::Task<> {
                    for (std::size_t i = 0; i < n; ++i) {
                        CHECK(co_await ws.recvJson<Test02Controller::Msg>() == recvMsg);
                    }
                    co_await ws.close();
                }) && [&]() -> coroutine::Task<> {
                    co_await loop->makeTimer().sleepFor(std::chrono::milliseconds{100});
                    for (std::size_t i = 0; i < n; ++i) {
                        std::string json;
                        recvMsg.id = 2233 ^ i * 2233 ^ 114514 ^ 996;
                        recvMsg.msg = net::internal::randomBase64();
                        recvMsg.name = net::internal::randomBase64();
                        reflection::toJson(recvMsg, json);
                        co_await client03.coPost(
                            makeUrl("127.0.0.1:28205/ws/addMsg"),
                            std::move(json),
                            HttpContentType::Json
                        );
                        co_await loop->makeTimer().sleepFor(std::chrono::milliseconds{200});
                    }
                }());
            co_await (client01.coClose() && client02.coClose() && client03.coClose()); 
        }());
    }
}

HX_CONTROLLER(Test03Controller) {
    HX_ENDPOINT_MAIN() {
        addEndpoint<POST>("/home", [] ENDPOINT {
            co_await res.setStatusAndContent(Status::CODE_200, "home")
                        .sendRes();
        });
        addEndpoint<POST>("/home/{id}", [] ENDPOINT {
            auto id = req.getPathParam(0).to<std::string>();
            co_await res.setStatusAndContent(Status::CODE_200, "/home/" + id)
                        .sendRes();
        });
        addEndpoint<POST>("/home/{id}/link/{that}", [] ENDPOINT {
            auto id = req.getPathParam(0).to<std::string>();
            auto that = req.getPathParam(1).to<std::string>();
            co_await res.setStatusAndContent(Status::CODE_200, id + that + "ok")
                        .sendRes();
        });
        addEndpoint<POST>("/home/op/{id}", [] ENDPOINT {
            auto id = req.getPathParam(0).to<std::string>();
            co_await res.setStatusAndContent(Status::CODE_200, "op:" + id)
                        .sendRes();
        });
        addEndpoint<POST>("/home/**", [] ENDPOINT {
            using namespace std::string_literals;
            auto all = req.getUniversalWildcardPath();
            co_await res.setStatusAndContent(Status::CODE_200, "all:"s += all)
                        .sendRes();
        });
    }
};

TEST_CASE("测试路径参数解析") {
    HttpServer server{28205};
    server.addController<Test03Controller>();
    server.asyncRun(1, initServerSsl, 1500_ms);
    auto client = makeClient();
    initClient(client);
#define HX_CLIENT_CHECK(__BODY__)                                              \
    if (!t) {                                                                  \
        sayErr(t.what());                                                      \
        CHECK(false);                                                          \
    } else {                                                                   \
        auto data = t.move();                                                  \
        if (data.status != 200) {                                              \
            sayErr("status != 200");                                           \
            log::hxLog.error(data);                                            \
            CHECK(false);                                                      \
        } else if (data.body != __BODY__) {                                    \
            sayErr("body != " __BODY__);                                       \
            CHECK(false);                                                      \
        }                                                                      \
    }
    {
        // 无参数
        auto t = client.post(
            makeUrl("127.0.0.1:28205/home"), "", JSON
        ).get();
        HX_CLIENT_CHECK("home");
    }
    {
        // 一个参数, 仅数字
        auto t = client.post(
            makeUrl("127.0.0.1:28205/home/123"), "", JSON
        ).get();
        HX_CLIENT_CHECK("/home/123");
    }
    {
        // 一个参数, 混乱
        auto t = client.post(
            makeUrl("127.0.0.1:28205/home/123dasdwaasdasdwasdwas2dwasdas88dwasdwfdcvxcvxcve2s"), "", JSON
        ).get();
        HX_CLIENT_CHECK("/home/123dasdwaasdasdwasdwas2dwasdas88dwasdwfdcvxcvxcve2s");
    }
    {
        // 两个参数
        auto t = client.post(
            makeUrl("127.0.0.1:28205/home/123/link/qwq"), "", JSON
        ).get();
        HX_CLIENT_CHECK("123qwqok");
    }
    {
        // 一个参数, 包含之前前缀
        auto t = client.post(
            makeUrl("127.0.0.1:28205/home/op/awa123"), "", JSON
        ).get();
        HX_CLIENT_CHECK("op:awa123");
    }
    {
        // 通配符
        auto t = client.post(
            makeUrl("127.0.0.1:28205/home/o1p/awa123/qwq666"), "", JSON
        ).get();
        HX_CLIENT_CHECK("all:/o1p/awa123/qwq666");
    }
#undef  HX_CLIENT_CHECK
}

#include <HXLibs/net/UnApiMacro.hpp>