#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-26 23:28:39
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
#ifndef _HX_UN_HX_API_H_
#define _HX_UN_HX_API_H_

#if defined(__GNUC__) || defined(__clang__)
#   pragma GCC diagnostic pop
#   pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#   pragma warning(pop)
#   pragma warning(pop)
#endif

#undef _EXPAND_2
#undef _EXPAND
#undef _UNIQUE_ID

#undef ROUTER
#undef ENDPOINT
#undef ROUTER_END

#undef ROUTER_ERROR_ENDPOINT
#undef RESPONSE_DATA
#undef RESPONSE_STATUS

#undef PARSE_PARAM
#undef PARSE_MULTI_LEVEL_PARAM
#undef GET_PARSE_QUERY_PARAMETERS

#undef ROUTER_BIND

#endif // !_HX_UN_HX_API_H_