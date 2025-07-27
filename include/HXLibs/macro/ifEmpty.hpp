#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-27 17:34:20
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
#ifndef _HX_MACRO_IF_EMPTY_H_
#define _HX_MACRO_IF_EMPTY_H_

#include <HXLibs/macro/join.hpp>

#define __HX_MACRO_CHECK_N__(x, n, ...) n

/**
 * @brief 获取传入的第二个参数, 作为返回, 一般应该传入一个参数
 * @note 通过一些判断, 拼接为 `__HX_MACRO_IS_EMPTY__` 宏, 来作为 1 返回.
 */
#define HX_CHECK(...) __HX_MACRO_CHECK_N__(__VA_ARGS__, 0, )
#define __HX_MACRO_IS_EMPTY__() ~, 1

// 判断是否为空
#define IF_EMPTY(x) HX_CHECK(HX_JOIN(__HX_MACRO_IS_EMPTY__, x)())

#endif // !_HX_MACRO_IF_EMPTY_H_