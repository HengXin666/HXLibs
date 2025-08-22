#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-27 17:37:41
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

#include <HXLibs/macro/IfEmpty.hpp>
#include <HXLibs/macro/Delay.hpp>

#define _hx_MACRO_FOR_IMPL_THIS__() _hx_MACRO_FOR_IMPL__
#define _hx_MACRO_FOR_IMPL__0(macro, x, ...) macro(x) HX_DELAY(_hx_MACRO_FOR_IMPL_THIS__)()(macro, __VA_ARGS__)
#define _hx_MACRO_FOR_IMPL__1(...)
#define _hx_MACRO_FOR_IMPL__(macro, x, ...) HX_JOIN(_hx_MACRO_FOR_IMPL__, IF_EMPTY(x))(macro, x, __VA_ARGS__)

/**
 * @brief for展开宏, 
 * @param macro 宏函数, 应该接受一个变量
 * @param x... 宏参数, 它们会被依次传入 macro 函数
 */
#define HX_FOR(macro, x, ...) HX_EVAL(_hx_MACRO_FOR_IMPL__(macro, x, __VA_ARGS__))

