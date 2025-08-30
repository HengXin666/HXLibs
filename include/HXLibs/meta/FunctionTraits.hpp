#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-08-29 17:00:00
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

#include <HXLibs/meta/TypeTraits.hpp>

namespace HX::meta {

namespace internal {

template <std::size_t Idx, typename... Args>
struct FunctionArg {
    using Arg = std::tuple_element_t<Idx, std::tuple<Args...>>;
};

// 主模板
template <typename... PT>
struct FunctionTraits;

// 函数指针
template <typename Res, typename... Args>
struct FunctionTraits<Res(*)(Args...)> {
    using ReturnType = Res;

    inline static constexpr std::size_t ArgCnt = sizeof...(Args);

    template <std::size_t Idx>
        requires (Idx < ArgCnt)
    using AtArg = FunctionArg<Idx, Args...>::Arg;
};

// 函数
template <typename Res, typename... Args>
struct FunctionTraits<Res(Args...)> : FunctionTraits<Res(*)(Args...)>{};

// 成员函数指针
template <typename C, typename Res, typename... Args>
struct FunctionTraits<Res(C::*)(Args...)> : FunctionTraits<Res(*)(Args...)>{};

// 成员函数指针
template <typename C, typename Res, typename... Args>
struct FunctionTraits<Res(C::*)(Args...) const> : FunctionTraits<Res(*)(Args...)>{};

// Lambda 的运算符重载的成员函数
template <typename Lambda>
struct FunctionTraits<Lambda> : FunctionTraits<decltype(&Lambda::operator())>{};

} // namespace internal

/**
 * @brief 获取函数对象的信息, 如返回值、参数、参数个数
 * @note 无法获取重载函数任何信息
 * @tparam Func 
 */
template <typename Func>
using FunctionInfo = internal::FunctionTraits<meta::remove_cvref_t<Func>>;

/**
 * @brief 获取函数的第 Idx 个参数的类型
 * @tparam Idx 
 * @tparam Func 
 */
template <std::size_t Idx, typename Func>
using FunctionAtArg = typename FunctionInfo<Func>::template AtArg<Idx>;

/**
 * @brief 获取函数的返回值
 * @tparam Func 
 */
template <typename Func>
using FunctionReturnType = typename FunctionInfo<Func>::ReturnType;

} // namespace HX::meta
