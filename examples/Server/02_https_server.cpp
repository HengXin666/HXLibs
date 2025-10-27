#include <HXLibs/net/Api.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

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

int main() {
    using namespace net;
    using namespace std::string_view_literals;
    HttpServer serv{"127.0.0.1", "28205"};
    
    // 站点`/`, 纯连接, 测试 返回头, 以及 返回 Hello World!
    serv.addEndpoint<GET>("/", [] ENDPOINT {
        co_await res.setStatusAndContent(Status::CODE_200, "Hello World!")
                    .sendRes();
    });

    // 站点`/mp4`, 测试大文件下载量 I/O `misaka.mp4` 大小为 574.6 MB (带断点续传)
    serv.addEndpoint<GET, HEAD>("/mp4/1", [] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "static/bigFile/misaka.mp4");
    });

    // 站点`/mp4`, 测试大文件下载量 I/O `misaka.mp4` 大小为 574.6 MB (ChunkedEncoding)
    serv.addEndpoint<GET>("/mp4/2", [] ENDPOINT {
        co_await res.useChunkedEncodingTransferFile("static/bigFile/misaka.mp4");
    });

    // 站点`/html/1`, 测试小文件 (普通I/O) [普通异步文件流读写]
    serv.addEndpoint<GET>("/html/1", [] ENDPOINT {
        co_await res.useRangeTransferFile(req.getRangeRequestView(), "static/bigFile/HXLibs.html");
    });

    // 站点`/html/2`, 测试小文件 (普通I/O) [ChunkedEncoding]
    serv.addEndpoint<GET>("/html/2", [] ENDPOINT {
        co_await res.useChunkedEncodingTransferFile("static/bigFile/HXLibs.html");
    });

    // echo
    serv.addEndpoint<GET>("/echo/**", [] ENDPOINT {
        log::hxLog.info("in echo: size =", req.getUniversalWildcardPath().size());
        co_return co_await res.setStatusAndContent(
            Status::CODE_200, req.getUniversalWildcardPath()
        ).sendRes();
    });

    // size
    serv.addEndpoint<GET>("/test/{size}", [] ENDPOINT {
        std::string buf;
        buf.resize(req.getPathParam(0).to<std::size_t>(), '!');
        co_return co_await res.setStatusAndContent(
            Status::CODE_200, buf
        ).sendRes();
    });

    // 分块编码 size
    serv.addEndpoint<GET>("/test2/{size}", [] ENDPOINT {
        std::string buf;
        buf.resize(req.getPathParam(0).to<std::size_t>(), '!');
        buf += '\n';
        buf += req.getPathParam(0);
        utils::AsyncFile file{req.getIO()};
        co_await file.open("./build/size.txt", utils::OpenMode::Write);
        co_await file.write(buf);
        co_await file.close();
        co_await res.useChunkedEncodingTransferFile("./build/size.txt");
    });

    // ws
    serv.addEndpoint<WS>("/ws", [] ENDPOINT {
        auto ws = co_await WebSocketFactory::accept(req, res);
        struct JsonDataVo {
            std::string msg;
            int code;
        };
        JsonDataVo const vo{"Hello 客户端, 我只能通信3次!", 200};
        co_await ws.sendJson(vo);
        for (int i = 0; i < 3; ++i) {
            auto res = co_await ws.recvText();
            log::hxLog.info(res);
            co_await ws.sendText("Hello! " + res);
        }
        co_await ws.close();
        log::hxLog.info("断开ws");
        co_return ;
    });

    serv.syncRun(1,[]{
#ifdef HXLIBS_ENABLE_SSL
        HX::net::SslContext::get().init({
            "certs/cert.pem",
            "certs/key.pem"
        }, true);
#endif // !HXLIBS_ENABLE_SSL
    });

    // using namespace std::chrono;
    // std::this_thread::sleep_for(10ms);

    // HttpClient client;
    // client.initSsl({
    //     "certs/cert.pem",
    //     "certs/key.pem"
    // });
    // // log::hxLog.info("get -> ", client.get("https://hengxin666.github.io/HXLoLi/").get().get());
    // log::hxLog.info("get -> ", client.get("https://127.0.0.1:28205/").get().get());

    // std::this_thread::sleep_for(3s);
}