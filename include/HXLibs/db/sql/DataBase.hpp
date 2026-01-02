#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-01-01 23:31:22
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

#include <HXLibs/db/sql/Column.hpp>
#include <HXLibs/db/sql/Dml.hpp>

namespace HX::db {

struct DateBase {
    template <Col... Cs>
    auto select() noexcept {
        return SelectBuild<DateBase&>{*this, {}}.select<Cs...>();
    }
};

} // namespace HX::db