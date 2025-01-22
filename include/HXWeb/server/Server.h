#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-08-22 22:44:53
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
#ifndef _HX_SERVER_H_
#define _HX_SERVER_H_

#include <string>
#include <chrono>
#include <thread>

namespace HX { namespace web { namespace server {

class ServerRun {
public:
    /**
     * @brief [阻塞的] 启动Http服务器, 并且阻塞
     * @param name 服务器绑定的地址, 如`127.0.0.1`
     * @param port 服务器绑定的端口, 如`28205`
     * @param threadNum 线程数 (相互独立)
     * @param timeout 超时断开时间
     */
    static void startHttp(
        const std::string& name,
        const std::string& port,
        std::size_t threadNum = std::thread::hardware_concurrency(),
        std::chrono::seconds timeout = std::chrono::seconds {30}
    );

    /**
     * @brief [阻塞的] 启动Http服务器, 并且阻塞
     * @param name 服务器绑定的地址, 如`127.0.0.1`
     * @param port 服务器绑定的端口, 如`28205`
     * @param certificate 公钥文件路径
     * @param privateKey 私钥文件路径
     * @param threadNum 线程数 (相互独立)
     * @param timeout 超时断开时间
     */
    static void startHttps(
        const std::string& name,
        const std::string& port,
        const std::string& certificate,
        const std::string& privateKey,
        std::size_t threadNum = std::thread::hardware_concurrency(),
        std::chrono::seconds timeout = std::chrono::seconds {30}
    );
};

}}} // namespace HX::web::server

#endif // !_HX_SERVER_H_