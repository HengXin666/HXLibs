#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-20 16:59:31
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _HX_WEB_SOCKET_H_
#define _HX_WEB_SOCKET_H_

#include <HXLibs/net/socket/IO.hpp>
#include <HXLibs/net/protocol/http/Request.hpp>
#include <HXLibs/net/protocol/http/Response.hpp>
#include <HXLibs/net/protocol/codec/SHA1.hpp>
#include <HXLibs/net/protocol/codec/Base64.hpp>
#include <HXLibs/utils/ByteUtils.hpp>

namespace HX::net {

namespace internal {

/**
 * @brief 官方要求的神秘仪式
 * @param userKey 用户发来的
 * @return std::string 
 */
inline std::string webSocketSecretHash(std::string userKey) {
    // websocket 官方要求的神秘仪式
/**
 * Sec-WebSocket-Key是随机的字符串，服务器端会用这些数据来构造出一个SHA-1的信息摘要。
 * 把“Sec-WebSocket-Key”加上一个特殊字符串“258EAFA5-E914-47DA-95CA-C5AB0DC85B11”，
 * 然后计算SHA-1摘要，之后进行Base64编码，将结果做为“Sec-WebSocket-Accept”头的值，返回给客户端。
 * 如此操作，可以尽量避免普通HTTP请求被误认为Websocket协议。
 * By https://zh.wikipedia.org/wiki/WebSocket
 */
    using namespace std::string_literals;
    userKey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"s;

    SHA1Context ctx;
    ctx.update(userKey.data(), userKey.size());

    uint8_t buf[sha1::DIGEST_BYTES];
    ctx.finish(buf);

    return base64Encode(buf);
}

} // namespace internal

/**
 * @todo 因为用户默认是希望群发, 最好实现一个连接池
 *
 * 但是因为要被动pong, 那么就需要后台在 read 挂着, 但是如果这个不是 ping, 那就是用户数据?
 */

/**
 * @brief WebSocket包
 */
struct WebSocketPacket {
    /**
     * @brief 操作码 (协议规定的!)
     */
    enum OpCode : uint8_t {
        Cont = 0,   // 附加数据帧
        Text = 1,   // 文本数据
        Binary = 2, // 二进制数据
        Close = 8,  // 关闭
        Ping = 9,
        Pong = 10,
    } opCode;

    /// @brief 内容
    std::string content;
};

class WebSocket {
public:
    WebSocket(IO& io)
        : _io{io}
    {}

    static coroutine::Task<WebSocket> accept(Request& req, Response& res) {
        using namespace std::string_literals;
        auto const& headMap = req.getHeaders();
        if (headMap.find("origin") == headMap.end()) {
            // Origin字段是必须的
            // 如果缺少origin字段, WebSocket服务器需要回复HTTP 403 状态码
            // https://web.archive.org/web/20170306081618/https://tools.ietf.org/html/rfc6455
            co_await res.setResLine(Status::CODE_403)
                        .sendRes();
            throw std::runtime_error{"The client is missing the origin field"};
        }
        if (auto it = headMap.find("upgrade"); it == headMap.end() || it->second != "websocket") {
            // 创建 WebSocket 连接失败
            throw std::runtime_error{"Failed to create a websocket connection"};
        }
        auto wsKey = headMap.find("sec-websocket-key");
        if (wsKey == headMap.end()) [[unlikely]] {
            // 在标头中找不到 sec-websocket-key
            throw std::runtime_error{"Not Find sec-websocket-key in headers"};
        }

        co_await res.setResLine(Status::CODE_101)
                    .addHeader("connection"s, "Upgrade")
                    .addHeader("upgrade"s, "websocket")
                    .addHeader("sec-websocket-accept"s, 
                               internal::webSocketSecretHash(wsKey->second))
                    .sendRes();

        co_return {req._io};
    }

private:
    IO& _io;

    template <typename Timeout>
        requires(requires { Timeout::Val; })
    auto serverRecvPacket() {
        return recvPacket<true, Timeout>();
    }

    template <typename Timeout>
        requires(requires { Timeout::Val; })
    auto clientRecvPacket() {
        return recvPacket<false, Timeout>();
    }

    auto serverSendPacket(WebSocketPacket&& packet) {
        return sendPacket<true>(std::move(packet), 0);
    }

    auto clientSendPacket(WebSocketPacket&& packet, uint32_t mask) {
        return sendPacket<false>(std::move(packet), mask);
    }

    template <bool isServer, typename Timeout>
        requires(requires { Timeout::Val; })
    coroutine::Task<std::optional<WebSocketPacket>> recvPacket() {
        WebSocketPacket packet;
        uint8_t head[2];
        auto res = co_await _io.recvLinkTimeout<Timeout>(head);
        if (res.index() == 1 || res.template get<0, exception::ExceptionMode::Nothrow> <= 0) [[unlikely]] {
            // 超时 或者 出错
            co_return std::nullopt;
        }
/*
   0               1               2               3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-------+-+-------------+-------------------------------+
  |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
  |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
  |N|V|V|V|       |S|             |   (if payload len==126/127)   |
  | |1|2|3|       |K|             |                               |
  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
  |     Extended payload length continued, if payload len == 127  |
  + - - - - - - - - - - - - - - - +-------------------------------+
  |                               |Masking-key, if MASK set to 1  |
  +-------------------------------+-------------------------------+
  | Masking-key (continued)       |          Payload Data         |
  +-------------------------------- - - - - - - - - - - - - - - - +
  :                     Payload Data continued ...                :
  + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
  |                     Payload Data continued ...                |
  +---------------------------------------------------------------+
  
  客户端发送 / 服务端解析:
  字节	  内容
  第1字节 FIN + OpCode
  第2字节 MASK=1, 长度
  后4字节 掩码 (Masking Key)
  数据	  每个字节 XOR 掩码

  服务器发送 / 客户端解析:
  字节	  内容
  第1字节 FIN + OpCode
  第2字节 MASK=0, 长度
  数据    真正的数据 (没有掩码字段)
*/
        bool fin;
        do {
            uint8_t head0 = head[0];
            uint8_t head1 = head[1];

            // 解析 FIN, 为 0 标识当前为最后一个包
            fin = (head0 & 0x80) != 0; // -127(十进制) & 1000 0000H => true

            // 解析 OpCode
            packet.opCode = static_cast<WebSocketPacket::OpCode>(head0 & 0x0F);
            
            // 求 MASK 如果非协议要求, 则是协议错误, 应该断开连接
            if ((head1 & 0x80) ^ isServer) [[unlikely]] {
                // 协议错误
                throw std::runtime_error{"Protocol Error"};
            }

            uint8_t payloadLen8 = head1 & 0x7F;
            if (packet.opCode >= 8 && packet.opCode <= 10) [[unlikely]] {
                throw std::runtime_error{"OpCode Illegal"};
            }

            // 解析包的长度
            std::size_t payloadLen;
            if (payloadLen8 == 0x7E) {
                uint16_t payloadLen16 = co_await _io.recvStruct<uint16_t>();
                payloadLen16 = utils::ByteUtils::byteswapIfLittle(payloadLen16);
                payloadLen = static_cast<std::size_t>(payloadLen16);
            } else if (payloadLen8 == 0x7F) {
                uint64_t payloadLen64 = co_await _io.recvStruct<uint64_t>();
                payloadLen64 = utils::ByteUtils::byteswapIfLittle(payloadLen64);
                if constexpr (sizeof(uint64_t) > sizeof(std::size_t)) {
                    if (payloadLen64 > std::numeric_limits<std::size_t>::max()) {
                        throw std::runtime_error{
                            "payloadLen64 > std::numeric_limits<size_t>::max()"};
                    }
                }
                payloadLen = static_cast<std::size_t>(payloadLen64);
            } else {
                payloadLen = static_cast<std::size_t>(payloadLen8);
            }

            if constexpr (isServer) {
                // 获取掩码
                uint32_t maskKey = co_await _io.recvStruct<uint32_t>();
                uint8_t mask[] {
                    static_cast<uint8_t>((maskKey >> 24) & 0xFF),
                    static_cast<uint8_t>((maskKey >> 16) & 0xFF),
                    static_cast<uint8_t>((maskKey >>  8) & 0xFF),
                    static_cast<uint8_t>((maskKey >>  0) & 0xFF)
                };
    
                // 解析数据
                std::string tmp;
                tmp.resize(payloadLen);
                co_await _io.fullyRecv(tmp);
    
                const std::size_t len = tmp.size();
                auto* p = reinterpret_cast<uint8_t*>(tmp.data());
                for (std::size_t i = 0; i != len; ++i) {
                    p[i] ^= mask[i % 4];
                }
                packet.content += std::move(tmp);
            } else {
                // 解析数据
                std::string tmp;
                tmp.resize(payloadLen);
                co_await _io.fullyRecv(tmp);
                packet.content += std::move(tmp);
            }
        } while (fin);
        co_return packet;
    }

    template <bool isServer>
    coroutine::Task<> sendPacket(
        WebSocketPacket&& packet,
        [[maybe_unused]] uint32_t mask
    ) {
        std::vector<uint8_t> data;
        if constexpr (isServer) {
            data.reserve(2 + 8); // 不发送掩码
        } else {
            data.reserve(2 + 8 + 4);
        }

        // 假设内容不会太大, 因为一个包可以最大存放是轻轻松松是 GB 级别的了 (1 << 63)
        constexpr bool fin = true;

        // head0
        data.push_back(fin << 7 | static_cast<uint8_t>(packet.opCode));

        // head1
        constexpr bool Mask = !isServer;
        // 设置长度
        uint8_t payloadLen8 = 0;
        if (packet.content.size() < 0x7E) {
            payloadLen8 = static_cast<uint8_t>(packet.content.size());
            data.push_back(Mask << 7 | static_cast<uint8_t>(payloadLen8));
        } else if (packet.content.size() <= 0xFFFF) {
            payloadLen8 = 0x7E;
            data.push_back(Mask << 7 | static_cast<uint8_t>(payloadLen8));
            auto payloadLen16 = static_cast<uint16_t>(packet.content.size());
            payloadLen16 = utils::ByteUtils::byteswapIfLittle(payloadLen16);
            auto* pLen = reinterpret_cast<uint8_t const*>(&payloadLen16);
            data.push_back(pLen[0]);
            data.push_back(pLen[1]);
        } else {
            payloadLen8 = 0x7F;
            data.push_back(Mask << 7 | static_cast<uint8_t>(payloadLen8));
            uint64_t len = static_cast<uint64_t>(packet.content.size());
            auto payloadLen64 = static_cast<uint64_t>(packet.content.size());
            payloadLen64 = utils::ByteUtils::byteswapIfLittle(payloadLen64);
            auto* pLen = reinterpret_cast<uint8_t const*>(&payloadLen64);
            for (std::size_t i = 0; i < 8; ++i) {
                data.push_back(pLen[i]);
            }
        }

        if constexpr (!isServer) {
            // 发送掩码, 注意掩码必需是客户端随机生成的
            uint8_t maskArr[] {
                static_cast<uint8_t>(mask >>  0 & 0xFF),
                static_cast<uint8_t>(mask >>  8 & 0xFF),
                static_cast<uint8_t>(mask >> 16 & 0xFF),
                static_cast<uint8_t>(mask >> 24 & 0xFF),
            };
            for (std::size_t i = 0; i < 4; ++i) {
                data.push_back(maskArr[i]);
            }

            // 使用掩码加密
            const std::size_t n = packet.content.size();
            for (std::size_t i = 0; i < n; ++i) {
                packet.content[i] ^= maskArr[i % 4];
            }
        }

        // 发送ws帧头
        co_await _io.fullySend(std::span<char const>{
            reinterpret_cast<char const*>(data.data()),
            data.size()
        });

        // 发送内容
        co_await _io.fullySend(packet.content);
    }
};

} // namespace HX::net

#endif // !_HX_WEB_SOCKET_H_