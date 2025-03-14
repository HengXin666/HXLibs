#include <HXWeb/protocol/websocket/WebSocket.h>

#include <hashlib/sha1.h>
#include <hashlib/base64.hpp>

#include <HXWeb/server/IO.h>
#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXSTL/coroutine/task/WhenAny.hpp>
#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXSTL/utils/ByteUtils.hpp>
#include <HXSTL/tools/ErrorHandlingTools.h>

namespace HX { namespace web { namespace protocol { namespace websocket {

/**
 * @brief 官方要求的神秘仪式
 * @param userKey 用户发来的
 * @return std::string 
 */
inline std::string _webSocketSecretHash(std::string userKey) {
    // websocket 官方要求的神秘仪式
/**
 * Sec-WebSocket-Key是随机的字符串，服务器端会用这些数据来构造出一个SHA-1的信息摘要。
 * 把“Sec-WebSocket-Key”加上一个特殊字符串“258EAFA5-E914-47DA-95CA-C5AB0DC85B11”，
 * 然后计算SHA-1摘要，之后进行Base64编码，将结果做为“Sec-WebSocket-Accept”头的值，返回给客户端。
 * 如此操作，可以尽量避免普通HTTP请求被误认为Websocket协议。
 * By https://zh.wikipedia.org/wiki/WebSocket
 */
    using namespace std::string_literals;
    SHA1 sha1;
    std::string inKey = userKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"s;
    sha1.add(inKey.data(), inKey.size());
    uint8_t buf[SHA1::HashBytes];
    sha1.getHash(buf);
    return base64::encode_into<std::string>(buf, buf + SHA1::HashBytes);
}

HX::STL::coroutine::task::Task<bool> WebSocket::httpUpgradeToWebSocket(
    const HX::web::server::IO<void>& io
) {
    using namespace std::string_literals;
    auto& headMap = io.getRequest().getRequestHeaders();
    if (auto it = headMap.find("upgrade"s); it == headMap.end() || it->second != "websocket"s) {
        co_return false;
    }

    // 是 ws:// 请求
    auto wsKey = headMap.find("sec-websocket-key"s);
    if (wsKey == headMap.end()) {
        // 怎么会有这种错误?! 什么乐色客户端?!
        io.getResponse().setResponseLine(HX::web::protocol::http::Status::CODE_400)
                        .setContentType(http::ResContentType::html)
                        .setBodyData("Not Find: sec-websocket-key"s);
        co_return false;
    }

    auto wsNewKey = _webSocketSecretHash(wsKey->second);

    io.getResponse().setResponseLine(HX::web::protocol::http::Status::CODE_101)
                    .addHeader("connection"s, "Upgrade"s)
                    .addHeader("upgrade"s, "websocket"s)
                    .addHeader("sec-websocket-accept"s, wsNewKey)
                    .setBodyData("");
    co_await io.sendResponse();
    co_return true;
    // https 的则是 wss:// ?!
}

HX::STL::coroutine::task::Task<WebSocket::pointer> WebSocket::makeServer(
    const HX::web::server::IO<void>& io
) {
    if (co_await httpUpgradeToWebSocket(io)) {
        co_return std::make_shared<pointer::element_type>(WebSocket {io});
    }
    co_return nullptr;
}

HX::STL::coroutine::task::Task<std::optional<WebSocketPacket>> WebSocket::recvPacket(
    std::chrono::steady_clock::duration timeout
) {
    WebSocketPacket packet;
    auto&& res = co_await HX::STL::coroutine::task::WhenAny::whenAny(
        HX::STL::coroutine::loop::TimerLoop::sleepFor(timeout),
        _io.recvN(2)
    );

    if (!res.index())
        co_return std::nullopt;

    auto&& head = std::get<1>(res);
    if (!head.has_value()) // 超时: 没有读取到数据
        co_return std::nullopt;
    bool fin;
    do {
        uint8_t head0 = static_cast<uint8_t>((*head)[0]);
        uint8_t head1 = static_cast<uint8_t>((*head)[1]);
        fin = (head0 & 0x80) != 0; // -127(十进制) & 1000 0000H => true
        packet._opCode = static_cast<WebSocketPacket::OpCode>(head0 & 0x0F);
        bool masked = (head1 & 0x80) != 0; //
        uint8_t payloadLen8 = head1 & 0x7F;
        std::size_t payloadLen;

        if (packet._opCode >= 8 && packet._opCode <= 10 && payloadLen8 >= 0x7E) [[unlikely]] {
            throw std::runtime_error{"packet._opCode >= 8 && packet._opCode <= 10 && payloadLen8 >= 0x7E"};
        }

        // 解析包的长度
        if (payloadLen8 == 0x7E) {
            uint16_t payloadLen16 = co_await _io.recvStruct<uint16_t>();
            payloadLen16 = HX::STL::utils::ByteUtils::byteswapIfLittle(payloadLen16);
            payloadLen = static_cast<size_t>(payloadLen16);
        } else if (payloadLen8 == 0x7F) {
            uint64_t payloadLen64 = co_await _io.recvStruct<uint64_t>();
            payloadLen64 = HX::STL::utils::ByteUtils::byteswapIfLittle(payloadLen64);
            payloadLen = static_cast<size_t>(payloadLen64);
            if constexpr (sizeof(uint64_t) > sizeof(size_t)) {
                if (payloadLen64 > std::numeric_limits<size_t>::max()) {
                    throw std::runtime_error{"payloadLen64 > std::numeric_limits<size_t>::max()"};
                }
            }
            payloadLen = static_cast<size_t>(payloadLen64);
        } else {
            payloadLen = static_cast<size_t>(payloadLen8);
        }

        std::string mask;
        if (masked) {
            mask = *co_await _io.recvN(4);
        }

        auto data = payloadLen ? *co_await _io.recvN(payloadLen) : "";

        if (masked) {
            const std::size_t len = data.size();
            for (std::size_t i = 0; i != len; ++i) {
                data[i] ^= mask[i % 4];
            }
        }
        packet._content += data;
    } while(!fin);
    co_return std::move(packet);
}

HX::STL::coroutine::task::Task<> WebSocket::sendPacket(
    WebSocketPacket packet,
    uint32_t mask /*= 0*/
) {
    std::string data;
    const bool fin = true;
    bool masked = mask != 0;
    uint8_t payloadLen8 = 0;

    if (packet._content.size() < 0x7E) {
        payloadLen8 = static_cast<uint8_t>(packet._content.size());
    } else if (packet._content.size() <= 0xFFFF) {
        payloadLen8 = 0x7E;
    } else {
        payloadLen8 = 0x7F;
    }

    uint8_t head0 = (fin ? 1 : 0) << 7 | static_cast<uint8_t>(packet._opCode);
    uint8_t head1 = (masked ? 1 : 0) << 7 | payloadLen8;
    char head[2];
    head[0] = static_cast<uint8_t>(head0);
    head[1] = static_cast<uint8_t>(head1);
    data.append(head, 2);

    if (packet._content.size() > 0x7E) {
        if (packet._content.size() <= 0xFFFF) {
            auto payloadLen16 = static_cast<uint16_t>(packet._content.size());
            payloadLen16 = HX::STL::utils::ByteUtils::byteswapIfLittle(payloadLen16);
            std::span<char const> pLen(reinterpret_cast<char const *>(std::addressof(payloadLen16)), sizeof(uint16_t));
            data.append(pLen.data(), pLen.size());
        } else {
            auto payloadLen64 = static_cast<uint64_t>(packet._content.size());
            payloadLen64 = HX::STL::utils::ByteUtils::byteswapIfLittle(payloadLen64);
            std::span<char const> pLen(reinterpret_cast<char const *>(std::addressof(payloadLen64)), sizeof(uint64_t));
            data.append(pLen.data(), pLen.size());
        }
    }

    if (masked) {
        char mask_buf[4];
        mask_buf[0] = mask >> 24;
        mask_buf[1] = (mask >> 16) & 0xFF;
        mask_buf[2] = (mask >> 8) & 0xFF;
        mask_buf[3] = mask & 0xFF;
        data.append(head, 4);

        const std::size_t len = packet._content.size();
        for (size_t i = 0; i != len; ++i) {
            packet._content[i] ^= mask_buf[i % 4];
        }
    }

    data += packet._content;
    co_await _io._sendSpan(data);
}

HX::STL::coroutine::task::Task<> WebSocket::sendPing() {
    co_await sendPacket(WebSocketPacket {
        ._opCode = WebSocketPacket::Ping,
        ._content = {},
    });
}

HX::STL::coroutine::task::Task<> WebSocket::start(
    std::chrono::steady_clock::duration pingPongTimeout /*= std::chrono::seconds(5)*/
) {
    while (true) {
        auto maybePacket = co_await recvPacket(pingPongTimeout);

        if (!maybePacket.has_value()) { // index == 1
            // 主动Ping
            if (_waitingPong) { // 上次ping还没有回复我呢! 对面已经嘎啦!
                break;
            }
            HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().startHostingTasks();
            co_await sendPing();
            _waitingPong = true;
            _lastPingTime = std::chrono::steady_clock::now();
            continue;
        }
        _waitingPong = false;

        auto&& packet = *maybePacket;

        switch (packet._opCode) {
        case WebSocketPacket::OpCode::Text: 
        case WebSocketPacket::OpCode::Binary: {
            // 收到消息
            if (_onMessage) {
                co_await _onMessage(packet._content);
            }
            break;
        }
        case WebSocketPacket::OpCode::Ping: {
            // 收到ping
            packet._opCode = WebSocketPacket::OpCode::Pong;
            co_await sendPacket(packet);
            break;
        }
        case WebSocketPacket::OpCode::Pong: {
            // 收到pong
            auto now = std::chrono::steady_clock::now();
            if (_onPong && _lastPingTime.time_since_epoch().count() != 0) {
                auto dt = now - _lastPingTime;
                co_await _onPong(dt);
            }
            break;
        }
        case WebSocketPacket::OpCode::Close: {
            // 收到关闭请求
            if (_onClose) {
                co_await _onClose();
            }

            if (_halfClosed) {
                co_return; // 退出, 由端点函数继续决定干什么
            }

            co_await sendPacket(packet);
            _halfClosed = true;
            break;
        }
        
        default:
            // printf("未知code: %u\n", packet._opCode);
            // co_return;
            break;
        }
    }
}

HX::STL::coroutine::task::Task<> WebSocket::send(const std::string& text) {
    if (_halfClosed) [[unlikely]] {
        throw std::runtime_error{"WebSocket is halfClosed"};
    }
    co_await sendPacket(WebSocketPacket {
        ._opCode = WebSocketPacket::Text,
        ._content = std::move(text),
    });
}

}}}} // HX::web::protocol::websocket