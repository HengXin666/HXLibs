#include <HXLibs/rpc/RpcClinet.hpp>
#include <HXLibs/rpc/RpcServer.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

int addTo(int a, int& b) { return b += a; }

coroutine::Task<std::string> addStr(std::string const& a, std::string& b) {
    co_return b += a;
}

int main() {
    rpc::RpcServer server{9090};
    server.bind<addTo>();
    server.bind<addStr>();
    server.getServer().asyncRun(1);
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    rpc::RpcClient cli{net::HttpClientOptions<>{}};
    cli.setBaseUrl("http://127.0.0.1:9090/rpc");
    {
        int to{1};
        auto res = coroutine::EventLoop{}.trySync(cli.req<addTo>(1, to));
        if (!res) [[unlikely]] {
            log::hxLog.error("???:", res.what());
            return 1;
        }
        log::hxLog.info("res =", res.get(), ", to =", to);
    }

    {
        std::string to{"1"};
        auto res = coroutine::EventLoop{}.trySync(cli.req<addStr>("1", to));
        if (!res) [[unlikely]] {
            log::hxLog.error("???:", res.what());
            return 1;
        }
        log::hxLog.info("res =", res.get(), ", to =", to);
    }
}