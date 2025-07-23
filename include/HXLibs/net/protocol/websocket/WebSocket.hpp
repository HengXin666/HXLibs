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
#include <HXLibs/net/protocol/url/UrlParse.hpp>
#include <HXLibs/net/protocol/codec/SHA1.hpp>
#include <HXLibs/net/protocol/codec/Base64.hpp>
#include <HXLibs/utils/ByteUtils.hpp>
#include <HXLibs/utils/Random.hpp>
#include <HXLibs/utils/TimeNTTP.hpp>
#include <HXLibs/reflection/json/JsonRead.hpp>
#include <HXLibs/reflection/json/JsonWrite.hpp>

#include <random>

namespace HX::net {

/**
 * @todo 专属 WS 的异常类型
 */

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

/**
 * @brief 生成一个 ws 的随机的 base64 字符串
 * @return std::string 
 */
inline std::string randomBase64() {
    std::array<uint8_t, 16> str;
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint16_t> uni(0, 255); // msvc 不支持 uint8_t 作为模版
    for (std::size_t i = 0; i < 16; ++i) {
        str[i] = static_cast<uint8_t>(uni(rng));
    }
    return base64Encode(str);
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
    Cont    = 0,    // 附加数据帧
    Text    = 1,    // 文本数据
    Binary  = 2,    // 二进制数据
    Close   = 8,    // 关闭
    Ping    = 9,    // ping
    Pong    = 10,   // ping
    Unknown = 255,  // 未初始化的
};

/**
 * @brief WebSocket包
 */
struct WebSocketPacket {
    OpCode opCode;

    /// @brief 内容
    std::string content{};
};

/**
 * @brief 该数据结构应该仅用于服务端发送
 */
struct WebSocketServerSendView {
    /// @brief 头部
    std::vector<uint8_t> head;

    /// @brief 内容
    std::string_view content{};
};

/**
 * @brief ws的模型: 服务端 / 客户端
 */
enum class WebSocketModel : bool {
    Client = false,
    Server = true
};

namespace internal {

/**
 * @brief 通过空基类优化掉服务端时候的随机数生成器
 * @note https://cppreference.cn/w/cpp/language/ebo
 */

template <WebSocketModel Model>
struct WebSocketBase {
    // 断言失败则是内部错误
    static_assert(Model == WebSocketModel::Server,
        "Internal error in the library");

    WebSocketBase() = default;

    WebSocketBase(std::size_t) 
        : WebSocketBase{} 
    {}
};

template <>
struct WebSocketBase<WebSocketModel::Client> {
    WebSocketBase() = default;

    WebSocketBase(std::size_t seed)
        : _random{seed}
    {}

    mutable std::optional<utils::XorShift32> _random;
};

} // namespace internal

template <WebSocketModel Model>
class WebSocket : public internal::WebSocketBase<Model> {
    using Base = internal::WebSocketBase<Model>;
    inline static constexpr bool IsServer = Model == WebSocketModel::Server;

public:
    // 默认Ping-Pong超时时间: 20s
    using DefaultPPTimeout = decltype(utils::operator""_s<'2', '0'>());

    // 默认读取数据的超时时间: 60s
    using DefaultRDTimeout = decltype(utils::operator""_s<'6', '0'>());

    WebSocket(IO& io)
        : Base{}
        , _io{io}
    {}

    WebSocket(IO& io, std::size_t seed)
        : Base{seed}
        , _io{io}
    {}

    /**
     * @brief 读取文本
     * @return coroutine::Task<std::string> 
     */
    coroutine::Task<std::string> recvText() const {
        co_return (co_await recv<false, DefaultRDTimeout>(OpCode::Text)).content;
    }

    /**
     * @brief 读取二进制
     * @return coroutine::Task<std::string> 
     */
    coroutine::Task<std::string> recvBytes() const {
        co_return (co_await recv<false, DefaultRDTimeout>(OpCode::Binary)).content;
    }

    /**
     * @brief 读取json并且序列化到结构体
     * @tparam T 
     * @param obj 
     * @return coroutine::Task<> 
     */
    template <typename T>
    coroutine::Task<> recvJson(T& obj) const {
        reflection::fromJson(
            obj,
            (co_await recv<false, DefaultRDTimeout>(OpCode::Text)).content
        );
    }

    /**
     * @brief 读取内容
     * @tparam TimeoutIsError 超时是否就是错误
          = true  则会抛异常, 
          = false 则是发送ping, 然后等待pong 再看是否超时, 而抛异常
     * @tparam Timeout 超时时间
     * @param recvType 读取的类型 (可选为: Text / Binary / Pong)
     * @param alternative 备选读取类型 (可选为: Text / Binary / Pong), Unknown 为无备选
     */
    template <bool TimeoutIsError = false, typename Timeout = DefaultPPTimeout>
        requires(requires { Timeout::Val; })
    coroutine::Task<WebSocketPacket> recv(OpCode recvType, OpCode alternative = OpCode::Unknown) const {
        for (;;) {
            auto res = co_await recvPacket<Timeout>();
            if (!res) [[unlikely]] {
                if constexpr (TimeoutIsError) {
                    // 读取超时
                    throw std::runtime_error{"Read Timeout"};
                } else {
                    // 第一次超时就 ping, 然后尝试读取 pong
                    co_await ping();
                    // 如果 pong 超时, 就是异常了!
                    auto res = co_await recv<true>(OpCode::Pong, recvType);
                    if (res.opCode == recvType) [[unlikely]] {
                        // 特别的: 如果此时 之前 recv 的数据到了, 那么继续把这次的 OpCode::Pong 接收
                        co_await recv<true>(OpCode::Pong);
                        co_return res;
                    }
                    // 没有超时就继续等待数据然后读取
                    continue;
                }
            } else if ((*res).opCode == OpCode::Ping) {
                co_await pong(std::move(*res).content);
                continue;
            } else if ((*res).opCode == OpCode::Close) [[unlikely]] {
                co_await resClose(std::move(*res).content);
                // 对方主动关闭连接: 安全关闭
                throw std::runtime_error{"Connection Closed OK: 1000"};
            } else if ((*res).opCode != recvType && (*res).opCode != alternative) [[unlikely]] {
                // 读取的不是期望的类型
                throw std::runtime_error{"The type read is not the expected type"};
            }
            co_return std::move(*res);
        }
    }

    /**
     * @brief 发送 ping
     * @param data 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> ping(std::string data = {}) const {
        return send(OpCode::Ping, std::move(data));
    }

    /**
     * @brief 发送文本
     * @param text 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> sendText(std::string text) const {
        co_await send(OpCode::Text, std::move(text));
    }

    /**
     * @brief 发送二进制
     * @param bytes 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> sendBytes(std::string bytes) const {
        co_await send(OpCode::Binary, std::move(bytes));
    }

    /**
     * @brief 发送 json
     * @tparam T 待序列化对象
     * @param obj 
     * @return coroutine::Task<> 
     */
    template <typename T>
    coroutine::Task<> sendJson(T const& obj) const {
        std::string json;
        reflection::toJson(obj, json);
        co_await send(OpCode::Text, std::move(json));
    }

    /**
     * @brief 发送数据
     * @param opCode 数据类型
     * @param str 数据
     * @return coroutine::Task<> 
     */
    coroutine::Task<> send(OpCode opCode, std::string str = {}) const {
        if constexpr (IsServer) {
            co_await sendPacket({
                opCode,
                std::move(str)
            }, 0);
        } else {
            co_await sendPacket({
                opCode,
                std::move(str)
            }, (*this->_random)());
        }
    }

    /**
     * @brief 主动关闭连接
     * @return coroutine::Task<> 
     */
    coroutine::Task<> close() const {
        // 发送断线协商
        co_await send(OpCode::Close);

        // 等待
        auto res = co_await recvPacket<DefaultPPTimeout>();

        if (!res || res->opCode != OpCode::Close) [[unlikely]] {
            // 超时 或者 协议非期望
            co_return ; // 那我假设已经完成了
        }

        // 再次发送, 确定断线
        co_await send(OpCode::Close);
    }

    /**
     * @brief 发送数据, 在群发的情况性能很高, 一次生成 + 多次发送
     * @param view 
     * @return coroutine::Task<> 
     */
    coroutine::Task<> sendPacketView(WebSocketServerSendView view) const {
        if constexpr (!IsServer) {
            // 该 API 为服务端特供
            static_assert(!sizeof(Model),
                "This API is exclusively provided for the server");
        }
        // 发送ws帧头
        co_await _io.fullySend(std::span<char const>{
            reinterpret_cast<char const*>(view.head.data()),
            view.head.size()
        });

        // 发送内容
        co_await _io.fullySend(view.content);
    }

private:
    IO& _io;

    // 响应 pong (无需暴露, 库内部使用即可)
    template <typename Timeout = DefaultPPTimeout>
        requires(requires { Timeout::Val; })
    coroutine::Task<> pong(std::string data) const {
        return send(OpCode::Pong, std::move(data));
    }

    // 被动关闭连接 (即对方发送了连接关闭)
    coroutine::Task<> resClose(std::string data) const {
        co_await send(OpCode::Close, std::move(data));
    }

    /**
     * @brief 读取并解析 ws 包
     * @tparam isServer 当前是否是服务端
     * @tparam Timeout 超时时间
     * @note 如果超时则返回 std::nullopt, 如果解析出错, 则抛异常
     */
    template <typename Timeout>
        requires(requires { Timeout::Val; })
    coroutine::Task<std::optional<WebSocketPacket>> recvPacket() const {
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
  以及, 如果分片 (fin = false), 那么它的第一个包是 (Test / Binary), 之后的只能是 Cont
  注意, 分片的数据只能是 Test / Binary
*/
        // 记录是否为最后一个分片
        bool fin;

        // 记录第一帧数据内容
        OpCode firstOpCode{OpCode::Unknown};
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
            fin = (head0 >> 7);

            // 解析 OpCode
            packet.opCode = static_cast<OpCode>(head0 & 0x0F);
            
            // 求 MASK 如果非协议要求, 则是协议错误, 应该断开连接
            bool mask = head1 & 0x80;
            if (mask ^ IsServer) [[unlikely]] {
                // 协议错误
                throw std::runtime_error{"Protocol Error"};
            }

            uint8_t payloadLen8 = head1 & 0x7F;
            if (static_cast<uint8_t>(packet.opCode) <= 2) {
                // 合法的数据帧
                if (firstOpCode == OpCode::Unknown) [[likely]] {
                    // 第一包必然是这里
                    firstOpCode = packet.opCode;
                } else if (packet.opCode != OpCode::Cont) [[unlikely]] {
                    // 分片情况下
                    throw std::runtime_error{"Fragmentation Error: OpCode != Cont"};
                }
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
                firstOpCode = packet.opCode;
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

            if constexpr (IsServer) {
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
        packet.opCode = firstOpCode;
        co_return packet;
    }

    /**
     * @brief 发送 ws 包
     * @param packet ws 包
     * @param mask 掩码 (每次需要随机生成, 仅客户端需要传参, 服务端传参无效)
     * @return coroutine::Task<> 
     */
    coroutine::Task<> sendPacket(
        WebSocketPacket&& packet,
        [[maybe_unused]] uint32_t mask
    ) const {
        std::vector<uint8_t> data;
        if constexpr (IsServer) {
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
        constexpr bool Mask = !IsServer;
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

        if constexpr (!IsServer) {
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

using WebSocketServer = WebSocket<WebSocketModel::Server>;
using WebSocketClient = WebSocket<WebSocketModel::Client>;

/**
 * @brief WebSocket的工厂方法
 */
class WebSocketFactory {
public:
    /**
     * @brief 服务端连接, 并且创建 ws 对象
     * @param req 
     * @param res 
     * @return coroutine::Task<WebSocket> 
     */
    static coroutine::Task<WebSocketServer> accept(Request& req, Response& res) {
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
        if (auto it = headMap.find("upgrade");
            it == headMap.end() || it->second != "websocket"
        ) [[unlikely]] {
            // 创建 WebSocket 连接失败
            co_await res.setResLine(Status::CODE_400)
                        .sendRes();
            throw std::runtime_error{"Failed to create a websocket connection"};
        }
        if (auto it = headMap.find("connection");
            it == headMap.end() || it->second != "Upgrade"
        ) [[unlikely]] {
            co_await res.setResLine(Status::CODE_416)
                        .sendRes();
            throw std::runtime_error{"Upgrade Required"};
        }
        auto wsKey = headMap.find("sec-websocket-key");
        if (wsKey == headMap.end()) [[unlikely]] {
            // 在标头中找不到 sec-websocket-key
            throw std::runtime_error{"Not Find sec-websocket-key in headers"};
        }

        co_await res.setResLine(Status::CODE_101)
                    .addHeader("Connection", "keep-alive, Upgrade")
                    .addHeader("Upgrade", "websocket")
                    .addHeader("Sec-Websocket-Accept", 
                               internal::webSocketSecretHash(wsKey->second))
                    .sendRes();

        co_return {req._io};
    }

    /**
     * @brief 创建 websocket 客户端
     * @tparam Timeout 创建连接请求的超时时间
     */
    template <typename Timeout>
        requires(requires { Timeout::Val; })
    static coroutine::Task<WebSocketClient> connect(std::string_view url, IO& io) {
        using namespace std::string_view_literals;
        // 发送 ws 升级协议
        Request req{io};
        auto key = internal::randomBase64();
        req.setReqLine<GET>(UrlParse::extractPath(url))
           .addHeaders("Origin", UrlParse::extractWsOrigin(url))
           .addHeaders("Connection", "Upgrade")
           .addHeaders("Upgrade", "websocket")
           .addHeaders("Sec-WebSocket-Key", key)
           .addHeaders("Sec-WebSocket-Version", "13");
        co_await req.sendHttpReq<Timeout>();
        // 解析响应
        Response res{io};
        if (co_await res.parserRes<Timeout>() == false) [[unlikely]] {
            throw std::runtime_error{"Timeout"};
        }
        // 校验
        auto const& headMap = res.getHeaders();
        if (auto it = headMap.find("connection");
            // 仅要求包含 Upgrade 即可
            it == headMap.end() || it->second.find("Upgrade") == std::string::npos
        ) [[unlikely]] {
            throw std::runtime_error{
                "Failed to create a websocket connection (Connection header invalid)"};
        }
        if (auto it = headMap.find("upgrade");
            it == headMap.end() || it->second != "websocket"
        ) [[unlikely]] {
            throw std::runtime_error{
                "Failed to create a websocket connection (Upgrade header invalid)"};
        }
        if (auto it = headMap.find("sec-websocket-accept");
            it == headMap.end() || it->second != internal::webSocketSecretHash(key)
        ) [[unlikely]] {
            throw std::runtime_error{
                "Failed to create a websocket connection (Accept hash mismatch)"};
        }
        // ws连接 成功
        co_return {io, std::random_device{}()};
    }

    /**
     * @brief 生成一个可以复用的发送包, 以减少拷贝开销
     * @warning 仅服务端可用
     * @param opCode 
     * @param msg 
     * @return WebSocketPacketView 
     */
    static WebSocketServerSendView makePacketView(OpCode opCode, std::string_view msg) {
        WebSocketServerSendView res{};
        std::vector<uint8_t>& data = res.head;
        data.reserve(2 + 8); // 不发送掩码

        // 假设内容不会太大, 因为一个包可以最大存放是轻轻松松是 GB 级别的了 (1 << 63)
        // 所以只有一个分片
        constexpr bool fin = true;

        // head0
        data.push_back(fin << 7 | static_cast<uint8_t>(opCode));

        // head1
        constexpr bool Mask = false;
        // 设置长度
        uint8_t payloadLen8 = 0;
        if (msg.size() < 0x7E) {
            payloadLen8 = static_cast<uint8_t>(msg.size());
            data.push_back(Mask << 7 | static_cast<uint8_t>(payloadLen8));
        } else if (msg.size() <= 0xFFFF) {
            payloadLen8 = 0x7E;
            data.push_back(Mask << 7 | static_cast<uint8_t>(payloadLen8));
            auto payloadLen16 = static_cast<uint16_t>(msg.size());
            payloadLen16 = utils::ByteUtils::byteswapIfLittle(payloadLen16);
            auto* pLen = reinterpret_cast<uint8_t const*>(&payloadLen16);
            data.push_back(pLen[0]);
            data.push_back(pLen[1]);
        } else {
            payloadLen8 = 0x7F;
            data.push_back(Mask << 7 | static_cast<uint8_t>(payloadLen8));
            auto payloadLen64 = static_cast<uint64_t>(msg.size());
            payloadLen64 = utils::ByteUtils::byteswapIfLittle(payloadLen64);
            auto* pLen = reinterpret_cast<uint8_t const*>(&payloadLen64);
            for (std::size_t i = 0; i < 8; ++i) {
                data.push_back(pLen[i]);
            }
        }

        res.content = msg;
        return res;
    }
};

// 断言: 大小不一样, 否则是库内部错误
static_assert(sizeof(WebSocketClient) != sizeof(WebSocketServer), 
    "Internal error in the library");

} // namespace HX::net

#endif // !_HX_WEB_SOCKET_H_