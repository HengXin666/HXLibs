#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-23 16:53:19
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
#ifndef _HX_DEFINED_H_
#define _HX_DEFINED_H_

/**
 * @brief 提供一些编译期常量, 以适配不同的平台 (完全就是为了低贱的win)
 * @note 尽可能的不引入宏!
 * @todo 目前这个毫无作用
 */

namespace HX::platform {

enum class OsType : int {
    Linux,
    Windows,
    Unknown
};

#if defined(__linux__)
    inline constexpr OsType NowOS = OsType::Linux;
#elif defined(_WIN32)
    inline constexpr OsType NowOS = OsType::Windows;
#else
    inline constexpr OsType NowOS = OsType::Unknown;
    #error "Unsupported operating system"
#endif

} // namespace HX::platform

#endif // !_HX_DEFINED_H_