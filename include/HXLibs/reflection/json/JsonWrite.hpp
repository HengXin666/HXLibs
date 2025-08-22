#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-16 19:30:27
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

#include <HXLibs/log/serialize/FormatString.hpp>
#include <HXLibs/log/serialize/ToString.hpp>

namespace HX::reflection {

/**
 * @brief 将obj反射为json字符串
 * @tparam IsFormat 是否格式化, 默认为 false (紧凑) 即不格式化
 * @tparam Obj 
 * @tparam Stream 要求支持 `.append()` 方法
 * @param obj 
 * @param s 
 */
template <bool IsFormat = false, typename Obj, typename Stream>
inline void toJson(Obj const& obj, Stream& s) {
    if constexpr (IsFormat) {
        log::formatString(obj, s);
    } else {
        log::toString(obj, s);
    }
}

} // namespace HX::reflection

