#include <HXHttp/HXResponse.h>

#include <sys/socket.h>

#include <HXprint/HXprint.h>

namespace HXHttp {

HXResponse::HXResponse(
    HXResponse::Status statusCode, 
    std::string_view describe /*= ""*/) : _statusLine("HTTP/1.1 ")
                                        , _responseHeaders()
                                        , _responseBody() {
    _statusLine.append(std::to_string(static_cast<int>(statusCode)) + ' ');
    if (!describe.size()) {
        switch (statusCode) {
            case HXResponse::Status::CODE_100:
                _statusLine.append("Continue");
                break;
            case HXResponse::Status::CODE_101:
                _statusLine.append("Switching Protocols");
                break;
            case HXResponse::Status::CODE_102:
                _statusLine.append("Processing");
                break;
            case HXResponse::Status::CODE_200:
                _statusLine.append("OK");
                break;
            case HXResponse::Status::CODE_201:
                _statusLine.append("Created");
                break;
            case HXResponse::Status::CODE_202:
                _statusLine.append("Accepted");
                break;
            case HXResponse::Status::CODE_203:
                _statusLine.append("Non-Authoritative Information");
                break;
            case HXResponse::Status::CODE_204:
                _statusLine.append("No Content");
                break;
            case HXResponse::Status::CODE_205: 
                _statusLine.append("Reset Content");
                break;
            case HXResponse::Status::CODE_206:
                _statusLine.append("Partial Content");
                break;
            case HXResponse::Status::CODE_207:
                _statusLine.append("Multi-Status");
                break;
            case HXResponse::Status::CODE_226:
                _statusLine.append("IM Used");
                break;
            case HXResponse::Status::CODE_300:
                _statusLine.append("Multiple Choices");
                break;
            case HXResponse::Status::CODE_301:
                _statusLine.append("Moved Permanently");
                break;
            case HXResponse::Status::CODE_302: 
                _statusLine.append("Moved Temporarily");
                break;
            case HXResponse::Status::CODE_303:
                _statusLine.append("See Other");
                break;
            case HXResponse::Status::CODE_304:
                _statusLine.append("Not Modified");
                break;
            case HXResponse::Status::CODE_305:
                _statusLine.append("Use Proxy");
                break;
            case HXResponse::Status::CODE_306:
                _statusLine.append("Reserved");
                break;
            case HXResponse::Status::CODE_307:
                _statusLine.append("Temporary Redirect");
                break;
            case HXResponse::Status::CODE_400:
                _statusLine.append("Bad Request");
                break;
            case HXResponse::Status::CODE_401:
                _statusLine.append("Unauthorized");
                break;
            case HXResponse::Status::CODE_402:
                _statusLine.append("Payment Required");
                break;
            case HXResponse::Status::CODE_403:
                _statusLine.append("Forbidden");
                break;
            case HXResponse::Status::CODE_404:
                _statusLine.append("Not Found");
                break;
            case HXResponse::Status::CODE_405:
                _statusLine.append("Method Not Allowed");
                break;
            case HXResponse::Status::CODE_406:
                _statusLine.append("Not Acceptable");
                break;
            case HXResponse::Status::CODE_407:
                _statusLine.append("Proxy Authentication Required");
                break;
            case HXResponse::Status::CODE_408:
                _statusLine.append("Request Timeout");
                break;
            case HXResponse::Status::CODE_409:
                _statusLine.append("Conflict");
                break;
            case HXResponse::Status::CODE_410:
                _statusLine.append("Gone");
                break;
            case HXResponse::Status::CODE_411:
                _statusLine.append("Length Required");
                break;
            case HXResponse::Status::CODE_412:
                _statusLine.append("Precondition Failed");
                break;
            case HXResponse::Status::CODE_413:
                _statusLine.append("Request Entity Too Large");
                break;
            case HXResponse::Status::CODE_414:
                _statusLine.append("Request-URI Too Large");
                break;
            case HXResponse::Status::CODE_415:
                _statusLine.append("Unsupported Media Type");
                break;
            case HXResponse::Status::CODE_416:
                _statusLine.append("Requested Range Not Satisfiable");
                break;
            case HXResponse::Status::CODE_417:
                _statusLine.append("Expectation Failed");
                break;
            case HXResponse::Status::CODE_422:
                _statusLine.append("Unprocessable Entity");
                break;
            case HXResponse::Status::CODE_423:
                _statusLine.append("Locked");
                break;
            case HXResponse::Status::CODE_424:
                _statusLine.append("Failed Dependency");
                break;
            case HXResponse::Status::CODE_425:
                _statusLine.append("Unordered Collection");
                break;
            case HXResponse::Status::CODE_426:
                _statusLine.append("Upgrade Required");
                break;
            case HXResponse::Status::CODE_428:
                _statusLine.append("Precondition Required");
                break;
            case HXResponse::Status::CODE_429:
                _statusLine.append("Too Many Requests");
                break;
            case HXResponse::Status::CODE_431:
                _statusLine.append("Request Header Fields Too Large");
                break;
            case HXResponse::Status::CODE_434:
                _statusLine.append("Requested host unavailable");
                break;
            case HXResponse::Status::CODE_444:
                _statusLine.append("Close connection without sending headers");
                break;
            case HXResponse::Status::CODE_449:
                _statusLine.append("Retry With");
                break;
            case HXResponse::Status::CODE_451:
                _statusLine.append("Unavailable For Legal Reasons");
                break;
            case HXResponse::Status::CODE_500:
                _statusLine.append("Internal Server Error");
                break;
            case HXResponse::Status::CODE_501:
                _statusLine.append("Not Implemented");
                break;
            case HXResponse::Status::CODE_502:
                _statusLine.append("Bad Gateway");
                break;
            case HXResponse::Status::CODE_503:
                _statusLine.append("Service Unavailable");
                break;
            case HXResponse::Status::CODE_504:
                _statusLine.append("Gateway Timeout");
                break;
            case HXResponse::Status::CODE_505:
                _statusLine.append("HTTP Version Not Supported");
                break;
            case HXResponse::Status::CODE_506:
                _statusLine.append("Variant Also Negotiates");
                break;
            case HXResponse::Status::CODE_507:
                _statusLine.append("Insufficient Storage");
                break;
            case HXResponse::Status::CODE_508:
                _statusLine.append("Loop Detected");
                break;
            case HXResponse::Status::CODE_509:
                _statusLine.append("Bandwidth Limit Exceeded");
                break;
            case HXResponse::Status::CODE_510:
                _statusLine.append("Not Extended");
                break;
            case HXResponse::Status::CODE_511: 
                _statusLine.append("Network Authentication Required");
                break;
            default: // 使用可真刁钻呐!
                break;
        }
    } else {
        _statusLine.append(describe);
    }
}

ssize_t HXResponse::sendResponse(int fd) const {
    std::string res {};
    res.append(_statusLine);
    res.append("\r\n");
    for (const auto& [key, val] : _responseHeaders) {
        res.append(key);
        res.append(": ");
        res.append(val);
        res.append("\r\n");
    }
    res.append("Content-Length: ");
    res.append(std::to_string(_responseBody.size()));
    res.append("\r\n\r\n");
    res.append(_responseBody);
    return ::send(fd, res.c_str(), res.size(), 0);
}

} // namespace HXHttp