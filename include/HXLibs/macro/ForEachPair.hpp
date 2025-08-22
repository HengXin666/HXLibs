#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-27 21:18:22
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

#define _hx_MACRO_FOR_EACH_PAIR_IMPL_THIS__() _hx_MACRO_FOR_EACH_PAIR_IMPL__
#define _hx_MACRO_FOR_EACH_PAIR__00(macro, arg1, arg2, ...) \
    macro(arg1, arg2) HX_DELAY(_hx_MACRO_FOR_EACH_PAIR_IMPL_THIS__)()(macro, __VA_ARGS__,)
#define _hx_MACRO_FOR_EACH_PAIR__11(...)
#define _hx_MACRO_FOR_EACH_PAIR_IMPL__(macro, arg1, arg2, ...) \
    HX_JOIN(_hx_MACRO_FOR_EACH_PAIR__, HX_JOIN(IF_EMPTY(arg1), IF_EMPTY(arg2))) \
    (macro, arg1, arg2, __VA_ARGS__)

/**
 * @brief forEachPair展开宏, 
 * @param macro 宏函数, 应该接受一个变量
 * @param (arg1, arg2)... 宏参数, 它们会被依次以 macro(arg1, arg2) 调用
 */
#define FOR_EACH_PAIR(macro, arg1, arg2, ...) \
    HX_EVAL(_hx_MACRO_FOR_EACH_PAIR_IMPL__(macro, arg1, arg2, __VA_ARGS__))

