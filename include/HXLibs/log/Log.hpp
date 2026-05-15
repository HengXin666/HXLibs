#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-07 13:18:50
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

#include <cstdio>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <io.h>
#endif

#include <HXLibs/log/serialize/FormatString.hpp>

namespace HX::log {

namespace internal {

#ifdef _WIN32
// 在 Windows 上, printf 会经过 CRT 的 locale 转换, 导致 UTF-8 中文乱码.
// 使用 WriteConsoleW 可以正确输出 Unicode 字符.
inline void printUtf8(const char* str) {
    if (auto h = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)));
        h != INVALID_HANDLE_VALUE && GetFileType(h) == FILE_TYPE_CHAR) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
        if (wlen > 0) {
            thread_local std::wstring wbuf;
            wbuf.resize(wlen - 1);
            MultiByteToWideChar(CP_UTF8, 0, str, -1, wbuf.data(), wlen);
            DWORD written;
            WriteConsoleW(h, wbuf.data(), static_cast<DWORD>(wbuf.size()), &written, nullptr);
            return;
        }
    }
    // 输出被重定向到文件时, 直接使用 printf
    fputs(str, stdout);
}
#else
inline void printUtf8(const char* str) {
    fputs(str, stdout);
}
#endif

struct Log {
    template <typename... Ts>
    void debug(Ts const&... ts) const {
#ifndef NDEBUG
        printUtf8("\033[1;35m[DEBUG]\033[0m \033[38;5;244m"); // 紫色标签 + 灰内容
        ((printUtf8((formatString(ts) + " ").c_str())), ...);
        printUtf8("\033[0m\n");
#else
        (static_cast<void>(ts), ...);
#endif
    }

    template <typename... Ts>
    void info(Ts const&... ts) const {
        printUtf8("\033[1;32m[INFO]\033[0m \033[32m"); // 亮绿标签 + 正常绿内容
        ((printUtf8((formatString(ts) + " ").c_str())), ...);
        printUtf8("\033[0m\n");
    }

    template <typename... Ts>
    void warning(Ts const&... ts) const {
        printUtf8("\033[1;33m[WARNING]\033[0m \033[33m"); // 亮黄标签 + 正黄内容
        ((printUtf8((formatString(ts) + " ").c_str())), ...);
        printUtf8("\033[0m\n");
    }

    template <typename... Ts>
    void error(Ts const&... ts) const {
        printUtf8("\033[1;31m[ERROR]\033[0m \033[31m"); // 亮红标签 + 正红内容
        ((printUtf8((formatString(ts) + " ").c_str())), ...);
        printUtf8("\033[0m\n");
    }
};

} // namespace internal

inline internal::Log hxLog;

} // namespace HX::log

