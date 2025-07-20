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
#include <HXLibs/utils/Random.hpp>
#include <HXLibs/utils/TimeNTTP.hpp>

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
 * @brief 操作码 (协议规定的!)
 */
enum class OpCode : uint8_t {
    Cont = 0,   // 附加数据帧
    Text = 1,   // 文本数据
    Binary = 2, // 二进制数据
    Close = 8,  // 关闭
    Ping = 9,
    Pong = 10,
};

/**
 * @brief WebSocket包
 */
struct WebSocketPacket {
    OpCode opCode;

    /// @brief 内容
    std::string content{};
};

class WebSocket {
    using DefaultTimeout = decltype(utils::operator""_s<'5'>());

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
public:
    WebSocket(IO& io)
        : _io{io}
        , _random{}
    {}

    WebSocket(IO& io, std::size_t seed)
        : _io{io}
        , _random{seed}
    {}

    template <typename Timeout = DefaultTimeout>
        requires(requires { Timeout::Val; })
    coroutine::Task<std::string> recv() {
        auto res = isClient() 
            ? co_await clientRecvPacket<Timeout>()
            : co_await serverRecvPacket<Timeout>();
        co_return (*res).content;
    }

    coroutine::Task<> send(OpCode opCode, std::string&& str) {
        isClient() 
            ? co_await clientSendPacket({
                    opCode,
                    std::move(str)
                }, (*_random)())
            : co_await serverSendPacket({
                    opCode,
                    std::move(str)
                });
    }

    /**
     * @brief 服务端连接, 并且创建 ws 对象
     * @param req 
     * @param res 
     * @return coroutine::Task<WebSocket> 
     */
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

    coroutine::Task<> close() {
        if (isClient()) {
            // 发送断线协商
            co_await clientSendPacket({
                OpCode::Close
            }, (*_random)());

            // 等待
            auto res = co_await clientRecvPacket<DefaultTimeout>();

            if (!res || res->opCode != OpCode::Close) [[unlikely]] {
                // 超时 或者 协议非期望
                co_return ; // 那我假设已经完成了
            }

            // 再次发送, 确定断线
            co_await clientSendPacket({
                OpCode::Close
            }, (*_random)());
        } else {
            // 发送断线协商
            co_await serverSendPacket({
                OpCode::Close
            });

            // 等待
            auto res = co_await serverRecvPacket<DefaultTimeout>();

            if (!res || res->opCode != OpCode::Close) [[unlikely]] {
                // 超时 或者 协议非期望
                co_return ; // 那我假设已经完成了
            }

            // 再次发送, 确定断线
            co_await serverSendPacket({
                OpCode::Close
            });
        }
    }

private:
    IO& _io;
    std::optional<utils::XorShift32> _random;

    // 判断当前是否为客户端
    bool isClient() const noexcept {
        return _random.has_value();
    }

    /**
     * @brief 读取并解析 ws 包
     * @tparam isServer 当前是否是服务端
     * @tparam Timeout 超时时间
     * @note 如果超时则返回 std::nullopt, 如果解析出错, 则抛异常
     */
    template <bool isServer, typename Timeout>
        requires(requires { Timeout::Val; })
    coroutine::Task<std::optional<WebSocketPacket>> recvPacket() {
        WebSocketPacket packet;
        uint8_t head[2];
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

  其中 fin = true 是最后一个分片, fin = false 是后面还有分片
*/
        bool fin;
        do {
            auto res = co_await _io.recvLinkTimeout<Timeout>(
                std::span<char>{reinterpret_cast<char*>(head), 2});
            if (res.index() == 1 
             || res.template get<0, exception::ExceptionMode::Nothrow>() <= 0
            ) [[unlikely]] {
                // 超时 或者 出错
                co_return std::nullopt;
            }
            uint8_t head0 = head[0];
            uint8_t head1 = head[1];

            // 解析 FIN, 为 0 标识当前为最后一个包
            fin = (head0 >> 7); // -127(十进制) & 1000 0000H => true

            // 解析 OpCode
            packet.opCode = static_cast<OpCode>(head0 & 0x0F);
            
            // 求 MASK 如果非协议要求, 则是协议错误, 应该断开连接
            bool mask = head1 & 0x80;
            if (mask ^ isServer) [[unlikely]] {
                // 协议错误
                throw std::runtime_error{"Protocol Error"};
            }

            uint8_t payloadLen8 = head1 & 0x7F;
            if (static_cast<uint8_t>(packet.opCode) <= 2) {
                // 合法的数据帧
            } else if (static_cast<uint8_t>(packet.opCode) >= 8 
                    && static_cast<uint8_t>(packet.opCode) <= 10
            ) {
                // 控制帧, 必须满足:
                if (!fin) [[unlikely]] {
                    throw std::runtime_error{
                        "Control frame cannot be fragmented"};
                }
                if (payloadLen8 >= 126) [[unlikely]] {
                    throw std::runtime_error{
                        "Control frame too big"};
                }
            } else [[unlikely]] {
                // 其他保留值, 非法
                throw std::runtime_error{"Unknown OpCode"};
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
                uint8_t maskKeyArr[4];
                co_await _io.fullyRecv(std::span<char>{
                    reinterpret_cast<char*>(maskKeyArr),
                    4
                });
    
                // 解析数据
                std::string tmp;
                tmp.resize(payloadLen);
                co_await _io.fullyRecv(tmp);
    
                const std::size_t len = tmp.size();
                auto* p = reinterpret_cast<uint8_t*>(tmp.data());
                for (std::size_t i = 0; i != len; ++i) {
                    p[i] ^= maskKeyArr[i % 4];
                }
                packet.content += std::move(tmp);
            } else {
                // 解析数据
                std::string tmp;
                tmp.resize(payloadLen);
                co_await _io.fullyRecv(tmp);
                packet.content += std::move(tmp);
            }
        } while (!fin);
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
        // 所以只有一个分片
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