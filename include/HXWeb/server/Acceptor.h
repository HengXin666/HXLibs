#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-31 11:54:38
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
 * */
#ifndef _HX_ACCEPTOR_H_
#define _HX_ACCEPTOR_H_

#include <memory>

#include <HXSTL/coroutine/task/Task.hpp>
#include <HXWeb/socket/AddressResolver.h>

namespace HX { namespace web { namespace server {

/**
 * @brief 连接接受类
 */
class Acceptor {
    socket::AddressResolver::Address _addr {}; // 用于存放客户端的地址信息 (在::accept中由操作系统填写; 可复用)
    using pointer = std::shared_ptr<Acceptor>;
public:
    /**
     * @brief 静态工厂方法
     * @return Acceptor指针
     */
    static pointer make() {
        return std::make_shared<pointer::element_type>();
    }

    /**
     * @brief 开始接受连接: 注册服务器套接字, 绑定并监听端口
     * @param name 主机名或地址字符串(IPv4 的点分十进制表示或 IPv6 的十六进制表示)
     * @param port 服务名可以是十进制的端口号, 也可以是已知的服务名称, 如 ftp、http 等
     */
    HX::STL::coroutine::task::Task<> start(const std::string& name, const std::string& port);
};

}}} // namespace HX::web::server

#endif // _HX_ACCEPTOR_H_
