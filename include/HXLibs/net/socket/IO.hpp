#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-07 21:09:45
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
#ifndef _HX_IO_H_
#define _HX_IO_H_

#include <string>

#include <HXLibs/coroutine/task/Task.hpp>

namespace HX::net {

class IO {
public:
    /**
     * @brief 设置响应体使用`TransferEncoding`分块编码, 以传输读取的文件
     * @param path 需要读取的文件的路径
     * @warning 在此之前都不需要使用`setBodyData`
     */
    coroutine::Task<> sendResponseWithChunkedEncoding(
        const std::string& path
    ) const;

    /**
     * @brief 使用`断点续传`, 以传输读取的文件. 内部会自动处理.
     * @param path 需要读取的文件的路径
     * @warning 在此之前都不需要使用`setBodyData` 
     */
    coroutine::Task<> sendResponseWithRange(
        const std::string& path
    ) const;
};

} // namespace HX::net

#endif // !_HX_IO_H_