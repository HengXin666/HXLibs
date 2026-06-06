#include <HXLibs/net/client/HttpClient.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

int main() {
    
    net::HeaderHashMap key{
        {"x-api-key", "sk-gwm-kILUyVm9JAOwx2n5NI9ddg_Wve82kymxv1zdqNMVx0g"}
    };
    
    auto loop = std::make_shared<coroutine::EventLoop>();
    loop->trySync([&]() -> coroutine::Task<> {
        net::HttpClient cli{net::HttpClientOptions{}, loop};
        {
            auto res = co_await cli.coGet("http://127.0.0.1:8792/v1/models", key);
            if (!res) {
                log::hxLog.error(res.what());
                co_return;
            }
            log::hxLog.info(res.get());
        }
        log::hxLog.debug("===================");
        {
            auto res = co_await cli.coPost(
                "http://127.0.0.1:8792/v1/chat/completions",
                R"({
    "model": "auto",
    "messages": [
      {
        "role": "system",
        "content": "You are a helpful assistant."
      },
      {
        "role": "user",
        "content": "Hello!"
      }
    ],
    "stream": false
  })",
                net::JSON,
                key
            );
            if (!res) {
                log::hxLog.error(res.what());
                co_return;
            }
            log::hxLog.info(res.get());
        }

        co_await cli.coClose();
    }());
}