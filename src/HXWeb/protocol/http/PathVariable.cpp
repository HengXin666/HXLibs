#include <HXWeb/protocol/http/PathVariable.h>

#include <HXWeb/protocol/http/Request.h>

namespace HX { namespace web { namespace protocol { namespace http {

PathVariable::PathVariable(Request& req) noexcept
    : _req(req) 
{
    _req._pathVariable = this;
}

PathVariable::PathVariable(Request& req, std::span<std::string_view> wda) noexcept
    : wildcarDataArr(wda)
    , _req(req)
{
    _req._pathVariable = this;
}

PathVariable::PathVariable(Request& req, std::string_view _UWPData) noexcept
    : UWPData(_UWPData)
    , _req(req)
{
    _req._pathVariable = this;
}

PathVariable::PathVariable(
    Request& req, 
    std::span<std::string_view> wda,
    std::string_view _UWPData
) noexcept
    : wildcarDataArr(wda)
    , UWPData(_UWPData)
    , _req(req)
{
    _req._pathVariable = this;
}

PathVariable::~PathVariable() noexcept {
    _req._pathVariable = nullptr;
}

}}}} // namespace HX::web::protocol::http
