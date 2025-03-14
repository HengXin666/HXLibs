#include <HXWeb/client/Client.h>

#include <openssl/ssl.h>

#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXWeb/protocol/proxy/ProxyBase.h>
#include <HXSTL/tools/ErrorHandlingTools.h>
#include <HXSTL/coroutine/loop/IoUringLoop.h>
#include <HXSTL/coroutine/loop/TimerLoop.h>
#include <HXSTL/coroutine/task/WhenAny.hpp>
#include <HXSTL/utils/FileUtils.h>
#include <HXSTL/utils/UrlUtils.h>

namespace HX { namespace web { namespace client {

HX::STL::coroutine::task::Task<
    std::shared_ptr<HX::web::protocol::http::Response>
> Client::request(
    const std::string& method,
    const std::string& url,
    const std::unordered_map<std::string, std::string> head /*= {}*/,
    const std::string& body /*= ""*/,
    std::chrono::milliseconds timeout /*= std::chrono::milliseconds {30 * 1000}*/,
    const std::string& proxy /*= ""*/,
    std::optional<HX::web::protocol::https::HttpsVerifyBuilder> verifyBuilder /*= std::nullopt*/
) {
    auto ptr = HX::web::client::Client::make();
    co_await ptr->start(url, proxy, timeout, verifyBuilder);
    ptr->_io->_request->setRequestLine(method, HX::STL::utils::UrlUtils::extractPath(url))
                       .addRequestHeaders(std::vector<std::pair<std::string, std::string>> {
                            {"Host", HX::STL::utils::UrlUtils::extractDomainName(url)},
                            {"Accept-Encoding", "identity"}, // 指定使用明文传输
                        })
                       .addRequestHeaders(head);
    if (body.size())
        ptr->_io->_request->setRequestBody(body);
    co_await ptr->_io->sendRequest();
    co_await ptr->_io->recvResponse(timeout);
    co_return std::make_shared<HX::web::protocol::http::Response>(
        std::move(ptr->_io->getResponse())
    );
}

HX::STL::coroutine::task::Task<> Client::start(
    const std::string& url,
    const std::string& proxy /*= ""*/,
    std::chrono::milliseconds timeout /*= std::chrono::milliseconds {5 * 1000}*/,
    std::optional<HX::web::protocol::https::HttpsVerifyBuilder> verifyBuilder /*= std::nullopt*/
) {
    socket::AddressResolver resolver;
    HX::STL::utils::UrlUtils::UrlInfoExtractor parser(proxy.size() ? proxy : url);
    auto entry = resolver.resolve(parser.getHostname(), parser.getService());
    auto sockaddr = entry.getAddress();
    
    int _clientFd = HX::STL::tools::UringErrorHandlingTools::throwingError(
        co_await HX::STL::coroutine::loop::IoUringTask().prepSocket(
            entry._curr->ai_family,
            entry._curr->ai_socktype,
            entry._curr->ai_protocol,
            0
        )
    );

    co_await HX::STL::coroutine::loop::IoUringTask().prepConnect(
        _clientFd,
        sockaddr._addr,
        sockaddr._addrlen
    );
    u_int16_t port = HX::STL::utils::UrlUtils::getProtocolPort(
        HX::STL::utils::UrlUtils::extractProtocol(url)
    );
    // 如何重构?
    if (port == 80) {
        _io = std::make_shared<HX::web::client::IO<HX::web::protocol::http::Http>>(
            _clientFd
        );
    } else if (port == 443) {
        _io = std::make_shared<HX::web::client::IO<HX::web::protocol::https::Https>>(
            _clientFd
        );

        // 如果是第一次使用, 则初始化 https::Context
        if (!HX::web::protocol::https::Context::getContext().getSslCtx()) {
            if (verifyBuilder.has_value()) {
                HX::web::protocol::https::Context::getContext().initClientSSL(*verifyBuilder);
            } else {
                HX::web::protocol::https::Context::getContext().initClientSSL({});
            }
        }
    } else [[unlikely]] {
        throw std::runtime_error{"Protocol is no in http(s)"};
    }
    if (proxy.size()) { // 进行代理连接
        co_await HX::web::protocol::proxy::ProxyBash::connect(proxy, url, *_io);
    }
    if (!co_await _io->init(timeout)) [[unlikely]] {
        throw std::runtime_error{"init Client Error"};
    }
}

HX::STL::coroutine::task::Task<bool> Client::read(std::chrono::milliseconds timeout) {
    auto&& res = co_await HX::STL::coroutine::task::WhenAny::whenAny(
        HX::STL::coroutine::loop::TimerLoop::sleepFor(timeout),
        _io->_recvResponse()
    );
    if (res.index())
        co_return std::get<1>(res);
    co_return true;
}

HX::STL::coroutine::task::Task<> Client::write(std::span<char> buf) {
    co_await _io->_sendSpan(buf);
}

}}} // namespace HX::web::client