#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-27 17:11:35
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
#ifndef _HX_MACRO_DELAY_H_
#define _HX_MACRO_DELAY_H_

#if 0
// 展开宏: 5^5 = 3125 次
#define HX_EVAL(...)                        \
    __HX_MACRO_EVAL1__(                     \
        __HX_MACRO_EVAL1__(                 \
            __HX_MACRO_EVAL1__(             \
                __HX_MACRO_EVAL1__(         \
                    __HX_MACRO_EVAL1__(     \
                        __VA_ARGS__         \
                    )))))
#define __HX_MACRO_EVAL1__(...)             \
    __HX_MACRO_EVAL2__(                     \
        __HX_MACRO_EVAL2__(                 \
            __HX_MACRO_EVAL2__(             \
                __HX_MACRO_EVAL2__(         \
                    __HX_MACRO_EVAL2__(     \
                        __VA_ARGS__         \
                    )))))
#define __HX_MACRO_EVAL2__(...)             \
    __HX_MACRO_EVAL3__(                     \
        __HX_MACRO_EVAL3__(                 \
            __HX_MACRO_EVAL3__(             \
                __HX_MACRO_EVAL3__(         \
                    __HX_MACRO_EVAL3__(     \
                        __VA_ARGS__         \
                    )))))
#define __HX_MACRO_EVAL3__(...)             \
    __HX_MACRO_EVAL4__(                     \
        __HX_MACRO_EVAL4__(                 \
            __HX_MACRO_EVAL4__(             \
                __HX_MACRO_EVAL4__(         \
                    __HX_MACRO_EVAL4__(     \
                        __VA_ARGS__         \
                    )))))
#define __HX_MACRO_EVAL4__(...)             \
    __HX_MACRO_EVAL5__(                     \
        __HX_MACRO_EVAL5__(                 \
            __HX_MACRO_EVAL5__(             \
                __HX_MACRO_EVAL5__(         \
                    __HX_MACRO_EVAL5__(     \
                        __VA_ARGS__         \
                    )))))
#define __HX_MACRO_EVAL5__(...) __VA_ARGS__
#endif

// 展开宏: 4^4 = 256 次 
#define HX_EVAL(...)                    \
    __HX_MACRO_EVAL1__(                 \
        __HX_MACRO_EVAL1__(             \
            __HX_MACRO_EVAL1__(         \
                __HX_MACRO_EVAL1__(     \
                    __VA_ARGS__         \
                ))))
#define __HX_MACRO_EVAL1__(...)         \
    __HX_MACRO_EVAL2__(                 \
        __HX_MACRO_EVAL2__(             \
            __HX_MACRO_EVAL2__(         \
                __HX_MACRO_EVAL2__(     \
                    __VA_ARGS__         \
                ))))
#define __HX_MACRO_EVAL2__(...)         \
    __HX_MACRO_EVAL3__(                 \
        __HX_MACRO_EVAL3__(             \
            __HX_MACRO_EVAL3__(         \
                __HX_MACRO_EVAL3__(     \
                    __VA_ARGS__         \
                ))))
#define __HX_MACRO_EVAL3__(...)         \
    __HX_MACRO_EVAL4__(                 \
        __HX_MACRO_EVAL4__(             \
            __HX_MACRO_EVAL4__(         \
                __HX_MACRO_EVAL4__(     \
                    __VA_ARGS__         \
                ))))
#if 0 // 展开 4^5 = 1024 次
#define __HX_MACRO_EVAL4__(...)         \
    __HX_MACRO_EVAL5__(                 \
        __HX_MACRO_EVAL5__(             \
            __HX_MACRO_EVAL5__(         \
                __HX_MACRO_EVAL5__(     \
                    __VA_ARGS__         \
                ))))
#define __HX_MACRO_EVAL5__(...) __VA_ARGS__
#else
#define __HX_MACRO_EVAL4__(...) __VA_ARGS__
#endif

// 延迟展开宏
#define __HX_MACRO_EMPTY__()
#define HX_DELAY(__MACRO_NAME__) __MACRO_NAME__ __HX_MACRO_EMPTY__()

#endif // !_HX_MACRO_DELAY_H_