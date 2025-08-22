#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-22 14:21:35
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

#include <HXLibs/net/socket/IO.hpp>
#include <HXLibs/net/protocol/url/UrlParse.hpp>

namespace HX::net {

template <typename T>
class Proxy {
public:
    using Type = T;

    Proxy(IO& io)
        : _io{io}
    {}

    coroutine::Task<> connect(std::string_view url, std::string_view targetUrl) {
        co_await static_cast<T*>(this)->connect(url, targetUrl);
    }
protected:
    IO& _io;
};

class Socks5Proxy : public Proxy<Socks5Proxy> {
public:
    using ProxyBase = Proxy<Socks5Proxy>;
    using ProxyBase::ProxyBase;

    coroutine::Task<> connect(std::string_view url, std::string_view targetUrl) {
        auto user = UrlParse::extractUser(url);
        co_await handshake(user.has_value());
        if (user) {
            co_await subNegotiation(user->account, user->password);
        }
        co_await socks5ConnectRequest(targetUrl);
    }
private:
    /**
     * @brief 子协商
     * @param username 
     * @param password 
     * @throw 失败
     */
    coroutine::Task<> subNegotiation(
        std::string_view username, 
        std::string_view password
    ) {
        // 发送用户名密码进行验证
        std::string authRequest;
        authRequest += static_cast<char>(0x01);
        authRequest += static_cast<char>(username.size());
        authRequest += username;
        authRequest += static_cast<char>(password.size());
        authRequest += password;
        co_await _io.fullySend(authRequest);

        char authResponse[2];
        co_await _io.fullySend(authResponse);
        if (authResponse[1] != 0x00) {
            throw std::invalid_argument("sub-negotiation: REP is " + std::to_string(authResponse[1]));
        }
    }

    /**
     * @brief 握手 | 协商
     * @param authentication 是否使用用户/密码进行验证
     * @throw 失败
     */
    coroutine::Task<> handshake(bool authentication) {
        char handshakeRequest[3] = { 
            0x05, // 协议版本号
            0x01, // 客户端支持的方法数量 (决定 METHODS 的长度)
            static_cast<char>(authentication ? 0x02 : 0x00)  // 不进行用户名密码验证
        };
        co_await _io.fullySend(handshakeRequest);

        // 解析服务端响应
        char handshakeResponse[2];
        co_await _io.fullyRecv(handshakeResponse);
        if (handshakeResponse[0] != 0x05                           // 协议版本 (需要一致)
         || handshakeResponse[1] != (authentication ? 0x02 : 0x00) // 服务端选择的可用方法
        ) [[unlikely]] {
            /**
             * @brief 身份验证方法(METHOD)的全部可选值如下:
             * 0x00 不需要身份验证(NO AUTHENTICATION REQUIRED)
             * 0x01 GSSAPI
             * 0x02 用户名密码(USERNAME/PASSWORD)
             * 0x03 至 0x7F 由 IANA 分配(IANA ASSIGNED)
             * 0x80 至 0xFE 为私人方法保留(RESERVED FOR PRIVATE METHODS)
             * 0xFF 无可接受的方法 (NO ACCEPTABLE METHODS)
             */
            throw std::invalid_argument("handshake: METHOD is " + std::to_string(handshakeResponse[1]));
        }
    }

    /**
     * @brief 发送代理请求
     * @param targetUrl 通过代理访问的目标服务器 URL
     * @throw 失败
     */
    coroutine::Task<> socks5ConnectRequest(std::string_view targetUrl) {
        std::string connectRequest;
        connectRequest += static_cast<char>(0x05); // 协议版本号 Version 5

        /**
        * @brief 命令类型
        * 0x01 CONNECT         | 代理 TCP 流量
        * 0x02 BIND            | 代理开启监听端口, 接收目标地址的连接
        *                      | (如果 SOCKS5 代理服务器具有公网 IP 地址, 则可以通过 BIND 请求实现内网穿透)
        * 0x03 UDP ASSOCIATE   | 代理 UDP 数据转发
        */
        connectRequest += static_cast<char>(0x01);
        
        connectRequest += static_cast<char>(0x00); // 保留字段

        /**
         * @brief 目标地址类型
         * 0x01 IPv4
         * 0x03 域名
         * 0x04 IPv6
         */
        connectRequest += static_cast<char>(0x03);

        /**
         * @brief 目标地址
         * 可变长度
         * 4 (IPv4)
         * 16 (IPv6)
         * 域名:
         *      如果 ATYP 字段值是 0x03，则 DST.ADDR 的格式为:
         *      - 域名长度 (一个unsigned char)
         *      - 域名 (unsigned char []) (可变长度)
         */
        UrlInfoExtractor parser(targetUrl);
        connectRequest += static_cast<char>(parser.getHostname().size());
        connectRequest += parser.getHostname();
        connectRequest.resize(connectRequest.size() + 2);
        *(reinterpret_cast<uint16_t*>(&connectRequest[connectRequest.size() - 2])) = 
            ::htons(UrlParse::getProtocolPort(parser.getService()));

        co_await _io.fullySend(connectRequest);
        co_await _io.fullyRecv(connectRequest);

        /**
         * @brief 
         * 字段	        描述            类型               长度       例值
         * VER          协议版本号	    unsigned char	    1	    0x05
         * REP          服务器应答	    unsigned char	    1	    0x00 成功
         * RSV          保留字段	    unsigned char	    1	    0x00
         * ATYP         目标地址类型	unsigned char	    1	    0x01 IPv4
         *                                                         0x04 IPv6
         * BND.ADDR	    绑定地址	    unsigned char []   可变长度
         *                                                4 (IPv4)
         *                                                16 (IPv6)	
         * BND.PORT	    绑定端口	    unsigned short	2
         */
        if (connectRequest[1] != 0x00) [[unlikely]] { // 失败
            /**
             * @brief 服务器响应消息中的 REP 字段如果不为 0x00, 则表示请求失. 不同值的具体含义如下:
             * 0x00 成功
             * 0x01 常规 SOCKS 服务器故障
             * 0x02 规则不允许的链接
             * 0x03 网络无法访问
             * 0x04 主机无法访问
             * 0x05 连接被拒绝
             * 0x06 TTL 过期
             * 0x07 不支持的命令
             * 0x08 不支持的地址类型
             */
            throw std::invalid_argument("Connect Request: REP is " + std::to_string(connectRequest[1]));
        }
    }
};

} // namespace HX::net

