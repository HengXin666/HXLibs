#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-19 00:05:37
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
#ifndef _HX_RANDOM_H_
#define _HX_RANDOM_H_

#include <cstdint>

namespace HX::utils {

struct XorShift32 {
    using result_type = uint32_t;
    result_type val;

    constexpr XorShift32(std::size_t seed = 0) 
        : val(static_cast<result_type>(seed + 1)) 
    {}

    constexpr result_type operator()() noexcept {
        result_type x = val;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return val = x;
    }
};

} // namespace HX::utils

#endif // !_HX_RANDOM_H_