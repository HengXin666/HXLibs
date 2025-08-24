#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-08-17 18:33:00
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

namespace HX::meta {

// 整数序列包
template <typename T, T... Is>
struct IntegerIndex {
    using Type = T;
};

namespace internal {

template <typename T, T Now, T End, T Stride, T... Is>
constexpr auto makeIntegerIndexByRange() {
    if constexpr (Now + Stride <= End) {
        return makeIntegerIndexByRange<T, Now + Stride, End, Stride, Is..., Now>();
    } else {
        return IntegerIndex<T, Is..., Now>{};
    }
}

} // namespace internal

/**
 * @brief 创建编译期整数序列, 于 [Begin, End]
 * @tparam T 整数类型
 * @tparam Begin 起始值
 * @tparam End 结束值 (包含)
 * @tparam Stride 步长
 */
template <typename T, T Begin, T End, T Stride = 1>
using makeIntegerIndexRange = decltype(internal::makeIntegerIndexByRange<T, Begin, End, Stride>());

} // namespace HX::meta

