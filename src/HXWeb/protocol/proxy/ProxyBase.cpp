#include <HXWeb/protocol/proxy/ProxyBase.h>

#include <HXSTL/utils/UrlUtils.h>
#include <HXWeb/protocol/proxy/SocksProxy.h>

namespace HX { namespace web { namespace protocol { namespace proxy {

HX::STL::coroutine::task::Task<> ProxyBash::connect(
    const std::string& url,
    const std::string& targetUrl,
    const HX::web::client::IO<void>& io
) {
    using namespace std::string_literals;
    std::string rmProtocolUrl = url;
    auto protocol = HX::STL::utils::UrlUtils::removeProtocol(rmProtocolUrl);
    if (protocol == "socks5"s) {
        co_await HX::web::protocol::proxy::Socks5Proxy(io)._connect(rmProtocolUrl, targetUrl);
    } else {
        throw std::invalid_argument("Does not support " + protocol + " proxy");
    }
}

}}}} // namespace HX::web::protocol::proxy