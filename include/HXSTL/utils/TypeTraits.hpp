#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-17 22:14:08
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
#ifndef _HX_TYPE_TRAITS_H_
#define _HX_TYPE_TRAITS_H_

#include <type_traits>

namespace HX { namespace STL { namespace utils {

/**
 * @brief 删除 T 类型的 const、引用、v 修饰
 * @tparam T 
 */
template <typename T>
using remove_cvref_v = std::remove_cv_t<std::remove_reference_t<T>>;

}}}

#endif // !_HX_TYPE_TRAITS_H_