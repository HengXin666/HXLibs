#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-08-12 10:05:15
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

#include <HXLibs/log/Log.hpp>

namespace HX::utils {

/**
 * @brief 简易时间计算工具, RAII的
 * @tparam T 时间单位 
 * @tparam Log 是否直接打印日志
 */
template <typename T = std::chrono::duration<double, std::milli>, bool Log = true>
class TickTock {
public:
    using double_ms = std::chrono::duration<double, std::milli>;
    using double_us = std::chrono::duration<double, std::micro>;
    using double_ns = std::chrono::duration<double, std::nano>;

    /**
     * @brief 构造一个记录时间的类, 并且立即开始计时
     * @param name 名称
     */
    explicit TickTock(std::string const& name)
        : _name(name)
        , _beginTime(std::chrono::steady_clock::now())
    {}

    ~TickTock() noexcept {
        auto endTime = std::chrono::steady_clock::now();
        auto dt = endTime - _beginTime;
        if constexpr (Log) {
            if constexpr (std::is_same_v<T, double_ms>) {
                log::hxLog.info(_name, ":", std::chrono::duration_cast<T>(dt).count(), "ms");
            } else if constexpr (std::is_same_v<T, double_us>) {
                log::hxLog.info(_name, ":", std::chrono::duration_cast<T>(dt).count(), "us");
            } else if constexpr (std::is_same_v<T, double_ns>) {
                log::hxLog.info(_name, ":", std::chrono::duration_cast<T>(dt).count(), "ns");
            } else {
                // time type (时间单位)
                log::hxLog.info(_name, ":", std::chrono::duration_cast<T>(dt).count(), "tt"); 
            }
        }
    }
private:
    std::string _name;
    decltype(std::chrono::steady_clock::now()) _beginTime;
};

template <typename T>
extern void notOptimized(T&&);

} // namespace HX::utils

