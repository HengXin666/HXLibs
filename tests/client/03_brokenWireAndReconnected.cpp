#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/net/Api.hpp>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace net;
using namespace utils;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#if 1
HttpServer server{"0.0.0.0", "28205"};

template <typename T>
struct JsonVO {
    int code;
    std::string msg;
    std::optional<T> t;
};

// 歌曲数据
struct MusicVO {
    uint64_t id;                        // 歌曲唯一ID
    std::string path;                   // 歌曲存放路径 (相对于 ~/file/music/)
    std::string musicName;              // 歌名
    std::vector<std::string> singers;   // 歌手
    std::string musicAlbum;             // 专辑
    uint64_t millisecondsLen;           // 毫秒长度
};

auto hx_init = [] {
    setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        auto cwd = std::filesystem::current_path();
        log::hxLog.debug("当前工作路径是:", cwd);
        if (cwd.filename() == "build") { // cmake
            std::filesystem::current_path("../static");
        } else {
            std::filesystem::current_path("../../../../static");
        }
        log::hxLog.debug("切换到路径:", std::filesystem::current_path());
        log::hxLog.debug(std::this_thread::get_id());
    } catch (const std::filesystem::filesystem_error& e) {
        log::hxLog.error("Error:", e.what());
    }
    server.addEndpoint<GET>("/", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "ok")
                    .sendRes();
    }).addEndpoint<GET>("/{id}", [] ENDPOINT {
        auto idStrView = req.getPathParam(0);
        uint64_t id;
        reflection::fromJson(id, idStrView);
        std::string json;
        reflection::toJson<true>(JsonVO<MusicVO>{
            0, "ok", MusicVO{
                id,
                "榊原ゆい - 刻司ル十二ノ盟約 (支配时间的十二盟约).flac",
                "刻司ル十二ノ盟約 (支配时间的十二盟约)",
                {"榊原ゆい"},
                "刻司ル十二",
                301320
            }
        }, json);
        co_await res.setBody(std::move(json))
                    .setResLine(net::Status::CODE_200)
                    .setContentType(net::JSON)
                    .sendRes();
        co_await res.getIO().close();
    }).addEndpoint<WS>("/ws", [] ENDPOINT {
        log::hxLog.warning("ws 端点");
        auto ws = co_await WebSocketFactory::accept(req, res);
        co_await ws.sendText("OK");
        co_await ws.close();
    }).addEndpoint<GET>("/ass", [] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "./ass/2.ass");
    });
    server.asyncRun(2, 100_ms);
    return 0;
}();
#endif

HttpClient client{HttpClientOptions<decltype(300_ms)>{}};

#if 1

TEST_CASE("测试朴素get的自动重连") {
    log::hxLog.warning("=== 测试朴素get的自动重连 ===");
    for (std::size_t _ = 0; _ < 5; ++_) {
        log::hxLog.info("连接ing...(", _, ")");
        log::hxLog.info("-->", client.get("http://0.0.0.0:28205/").get().move().body);
        std::this_thread::sleep_for(decltype(200_ms)::Val); // 等待 0.2s
        // 此时客户端已经被服务端断线了
    }
    std::this_thread::sleep_for(decltype(200_ms)::Val); // 期望超时?
}

TEST_CASE("测试get变长body的自动重连") {
    log::hxLog.warning("=== 测试get变长body的自动重连 ===");
    for (std::size_t _ = 0; _ < 5; ++_) {
        log::hxLog.info("连接ing...(", _, ")");
        log::hxLog.info("-->", client.get("http://0.0.0.0:28205/" 
            + std::to_string((uint64_t)std::pow(5 - _, 10))).get().move().body);
        std::this_thread::sleep_for(decltype(200_ms)::Val); // 等待 0.2s
        // 此时客户端已经被服务端断线了
    }
}

TEST_CASE("测试ws") {
    log::hxLog.warning("=== 测试ws ===");
    client.wsLoop("ws://0.0.0.0:28205/ws", [](WebSocketClient ws) -> coroutine::Task<> {
        auto txt = co_await ws.recvText();
        log::hxLog.info("ws:", txt);
        co_await ws.close();
    }).thenTry([](container::Try<> t) {
        if (!t) [[unlikely]] {
            log::hxLog.error("ws:", t.what());
        }
    });
}

#endif

TEST_CASE("测试 file + get 交替") {
    log::hxLog.warning("=== 测试 file + get 交替 ===");
    
    for (std::size_t _ = 0; _ < 5; ++_) {
        log::hxLog.info("连接ing...(", _, ")");
        log::hxLog.info("-->", client.get("http://0.0.0.0:28205/ass").get().move().body.size() 
            == utils::FileUtils::getFileSize("./ass/2.ass"));
        std::this_thread::sleep_for(decltype(200_ms)::Val); // 等待 0.2s
        log::hxLog.info("-->", client.get("http://0.0.0.0:28205/" 
            + std::to_string((uint64_t)std::pow(5 - _, 10))).get().move().body);
        std::this_thread::sleep_for(decltype(200_ms)::Val); // 等待 0.2s
        // 此时客户端已经被服务端断线了
    }
}