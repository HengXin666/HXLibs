#include <HXWeb/server/ConnectionHandler.h>

#include <poll.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>  
#include <openssl/err.h>

#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXSTL/coroutine/task/WhenAny.hpp>
#include <HXSTL/tools/ErrorHandlingTools.h>
#include <HXSTL/utils/FileUtils.h>
#include <HXWeb/router/Router.h>
#include <HXWeb/interceptor/RequestInterceptor.h>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXWeb/server/IO.h>
#include <HXprint/print.h>

namespace HX { namespace web { namespace server {

HX::STL::coroutine::task::TimerTask ConnectionHandler<HX::web::protocol::http::Http>::start(
    int fd, 
    std::chrono::seconds timeout
) {
    {
        HX::web::server::IO<HX::web::protocol::http::Http> io {fd};
        bool endpointRes = 0; // 是否复用连接

        while (true) {
            // === 读取 ===
            // LOG_INFO("读取中...");
            {
                auto&& res = co_await HX::STL::coroutine::task::WhenAny::whenAny(
                    HX::STL::coroutine::loop::TimerLoop::sleepFor(timeout),
                    io._recvRequest()
                );

                if (!res.index())
                    goto END;

                if (std::get<1>(res)) {
                    LOG_INFO("客户端 %d 已断开连接!", fd);
                    co_return;
                }
            }
            // === 路由解析 ===
            // 交给路由处理
            // LOG_INFO("路由解析中...");
            try {
                // 前向拦截
                if (HX::web::interceptor::RequestInterceptor::
                        getRequestInterceptor()
                            .checkPreHandle(io) ==
                    HX::web::interceptor::RequestFlow::Block
                ) {
                    co_await io.sendResponse(HX::STL::container::NonVoidHelper<>{});
                    break;
                }
                // printf("cli -> url: %s\n", _request.getRequesPath().c_str());
                endpointRes = co_await HX::web::router::Router::getSingleton().getEndpointFunc(
                    io._request->getRequesType(),
                    io._request->getRequesPath()
                )(io);

                // === 响应 ===
                // LOG_INFO("响应中...");
                co_await io.sendResponse(HX::STL::container::NonVoidHelper<>{});
                if (!endpointRes)
                    break;
            } catch (const std::exception& e) {
                LOG_ERROR("向客户端 %d 发送消息时候出错 (e): %s", fd, e.what());
                break;
            } catch (const char* msg) {
                LOG_ERROR("向客户端 %d 发送消息时候出错 (msg): %s", fd, msg);
                break;
            } catch(...) {
                LOG_ERROR("向客户端 %d 发送消息时候发生未知错误", fd);
                break;
            }
        }
    }
END:
    HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().startHostingTasks();
    LOG_WARNING("客户端直接给我退出! (%d)", fd);
}

HX::STL::coroutine::task::TimerTask ConnectionHandler<HX::web::protocol::https::Https>::start(
    int fd,
    std::chrono::seconds timeout
) {
    {
        HX::web::server::IO<HX::web::protocol::https::Https> io {fd};
        bool endpointRes = 0; // 是否复用连接

        {    
            // SSL 握手
            auto&& res = co_await HX::STL::coroutine::task::WhenAny::whenAny(
                HX::STL::coroutine::loop::TimerLoop::sleepFor(timeout),
                io.handshake()
            );

            if (!res.index())
                goto END;
        }

        while (true) {
            // === 读取 ===
            // LOG_INFO("读取中...");
            {
                auto&& res = co_await HX::STL::coroutine::task::WhenAny::whenAny(
                    HX::STL::coroutine::loop::TimerLoop::sleepFor(timeout),
                    io._recvRequest()
                );

                if (!res.index())
                    goto END;

                if (std::get<1>(res)) {
                    LOG_INFO("客户端 %d 已断开连接!", fd);
                    co_return;
                }
            }
            // === 路由解析 ===
            // 交给路由处理
            // LOG_INFO("路由解析中...");
            try {
                // 前向拦截
                if (HX::web::interceptor::RequestInterceptor::
                        getRequestInterceptor()
                            .checkPreHandle(io) ==
                    HX::web::interceptor::RequestFlow::Block
                ) {
                    co_await io.sendResponse(HX::STL::container::NonVoidHelper<>{});
                    break;
                }

                endpointRes = co_await HX::web::router::Router::getSingleton().getEndpointFunc(
                    io._request->getRequesType(),
                    io._request->getRequesPath()
                )(io);

                // printf("cli -> url: %s\n", _request.getRequesPath().c_str());
                // === 响应 ===
                // LOG_INFO("响应中...");
                co_await io.sendResponse(HX::STL::container::NonVoidHelper<>{});
                if (!endpointRes)
                    break;
            } catch (const std::exception& e) {
                LOG_ERROR("向客户端 %d 发送消息时候出错 (e): %s", fd, e.what());
                break;
            } catch (const char* msg) {
                LOG_ERROR("向客户端 %d 发送消息时候出错 (msg): %s", fd, msg);
                break;
            } catch(...) {
                LOG_ERROR("向客户端 %d 发送消息时候发生未知错误", fd);
                break;
            }
        }
    }
END:
    HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().startHostingTasks();
    LOG_WARNING("客户端直接给我退出! (%d)", fd);
}

}}} // namespace HX::web::server
