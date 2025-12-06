#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-11-29 21:58:59
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

#include <type_traits>

namespace HX::meta {

namespace internal {

template <typename T>
struct GetMemberPtrTypeImpl;

template <typename T, typename ClassT>
struct GetMemberPtrTypeImpl<T ClassT::*> {
    using Type = T;
    using ClassType = ClassT;
};

} // namespace internal

/**
 * @brief 判断类型是否为成员指针类型
 * @tparam T 
 */
template <typename T>
bool constexpr IsMemberPtrVal = false;

template <typename T, typename ClassPtr>
bool constexpr IsMemberPtrVal<T ClassPtr::*> = true;

/**
 * @brief 获取成员指针指向的元素的类型
 * @tparam T 
 */
template <typename T>
using GetMemberPtrType = typename internal::GetMemberPtrTypeImpl<T>::Type;

/**
 * @brief 获取成员指针的类的类型
 * @tparam T 
 */
template <typename T>
using GetMemberPtrClassType = typename internal::GetMemberPtrTypeImpl<T>::ClassType;

/**
 * @brief 获取成员指针们的类的类型, 并且保证这些指针都来自于同一个类
 * @tparam MemberPtr 
 * @tparam MemberPtrs
 */
template <typename... MemberPtrTs>
using GetMemberPtrsClassType = decltype([] <typename MP, typename... MemberPtrs> (MP, MemberPtrs...) {
    // 别名模板不支持部分推导
    // 不能 <typename MP, typename... MemberPtrs> 然后直接让 <typename... MemberPtrTs> 匹配
    // 只能拆开
    using MemberPtr = MP;
    return std::conditional_t<
    (std::is_same_v<
        GetMemberPtrClassType<MemberPtr>,
        GetMemberPtrClassType<MemberPtrs>
    > && ...),
    GetMemberPtrClassType<MemberPtr>, void> {};
} (MemberPtrTs{}...));

} // namespace HX::meta