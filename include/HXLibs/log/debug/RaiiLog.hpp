#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-05-30 15:56:39
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

namespace HX::log {

template <auto V, auto Level = &internal::Log::warning<decltype(V), int64_t, std::string_view>>
struct RaiiLog {
    RaiiLog() {
        using namespace std::string_view_literals;
        (hxLog.*Level)(V, ++cnt, ": RaiiLog === new === {"sv);
    }
    
    ~RaiiLog() {
        using namespace std::string_view_literals;
        (hxLog.*Level)(V, --cnt, ": } // RaiiLog === del ==="sv);
    }

    inline static int64_t cnt{};
};

} // namespace HX::log
