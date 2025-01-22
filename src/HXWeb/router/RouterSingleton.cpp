#include <HXWeb/router/RouterSingleton.h>

#include <HXWeb/server/IO.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXSTL/utils/StringUtils.h>

namespace HX { namespace web { namespace router {

RouterSingleton::RouterSingleton()
    : _routerRadixTree()
    , _errorEndpointFunc(
        [=](const HX::web::server::IO<>& io) -> HX::web::router::RouterSingleton::EndpointReturnType {
            io.getResponse().setResponseLine(HX::web::protocol::http::Response::Status::CODE_404)
                            .setContentType("text/html", "UTF-8")
                            .setBodyData("<!DOCTYPE html><html><head><meta charset=UTF-8><title>404 Not Found</title><style>body{font-family:Arial,sans-serif;text-align:center;padding:50px;background-color:#f4f4f4}h1{font-size:100px;margin:0;color:#333}p{font-size:24px;color:#666}</style><body><h1>404</h1><p>Not Found</p><hr/><p>HXNet</p>");
            co_return false;
        }
    )
{}

void RouterSingleton::addEndpoint(
    const std::string& requestType, 
    const std::string& path, 
    const RouterSingleton::EndpointFunc& func
    ) {
    _routerRadixTree.insert(HX::STL::utils::StringUtil::split(path, "/", {requestType}), func);
}

RouterSingleton::EndpointFunc RouterSingleton::getEndpointFunc(const std::string& requestType, const std::string& path) const {
    // 如果path是`/home/?loli=imouto`这种, 先不要解析参数, 应该只做?之前的映射(没有必要, 因为用户可以乱传输后面的参数)
    std::size_t pos = path.find('?');
    if (auto res = _routerRadixTree.find(HX::STL::utils::StringUtil::split(
        pos == std::string::npos ? path : path.substr(0, pos), "/", {requestType}
    ))) {
        return *res;
    }
    return _errorEndpointFunc;
}

}}} // namespace HX::web::router
