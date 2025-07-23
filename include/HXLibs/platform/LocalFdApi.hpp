#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-09 10:44:39
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
#ifndef _HX_LOCAL_FD_API_H_
#define _HX_LOCAL_FD_API_H_

/**
 * @brief 跨平台 本地 fd 类型定义
 * @note 只定义 local_fd_t, 不引入额外命名、宏或接口
 * @note 不要问我为什么, 你问一下win的架构师?
 */

#if defined(__linux__)
    #include <unistd.h>
    #include <fcntl.h>

    namespace HX::platform {
        using LocalFdType = int;
        inline constexpr LocalFdType kInvalidLocalFd = -1;

        enum class OpenMode : int {
            Read = O_RDONLY | kOpenModeDefaultFlags,                        // 只读模式 (r)
            Write = O_WRONLY | O_TRUNC | O_CREAT | kOpenModeDefaultFlags,   // 只写模式 (w)
            ReadWrite = O_RDWR | O_CREAT | kOpenModeDefaultFlags,           // 读写模式 (a+)
            Append = O_WRONLY | O_APPEND | O_CREAT | kOpenModeDefaultFlags, // 追加模式 (w+)
            Directory = O_RDONLY | O_DIRECTORY | kOpenModeDefaultFlags,     // 目录
        };

        using ModeType = ::mode_t;
    } // namespace HX::platform
#elif defined(_WIN32)
    #include <string>
    #include <string_view>
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
    #include <winternl.h>

    #pragma comment(lib, "ntdll.lib")

    #ifndef AT_FDCWD
        #define AT_FDCWD 0721
    #endif // !AT_FDCWD

    namespace HX::platform {

        using LocalFdType = ::HANDLE; // void*

        namespace internal {
        
            // 因为 INVALID_HANDLE_VALUE 不能被初始化为编译期常量, 甚至常量都不允许!
            // So Windows fuck you! 设计的什么垃圾api 还宏 还 void*, 八嘎呀路!
            struct __FUCK_WINDOWS_INVALID_HANDLE_VALUE {
                // 为了可以 fd = kInvalidLocalFd
                operator LocalFdType() const noexcept {
                    return INVALID_HANDLE_VALUE;
                }
            };
            
            /// @note 需要写在 internal 里面, 不然不会 ADL 查找, 那样就需要手动 using 命名空间了

            inline bool operator==(LocalFdType fd,
                                   __FUCK_WINDOWS_INVALID_HANDLE_VALUE) noexcept {
                return fd == INVALID_HANDLE_VALUE;
            }

            inline bool operator==(__FUCK_WINDOWS_INVALID_HANDLE_VALUE,
                                   LocalFdType fd) noexcept {
                return fd == INVALID_HANDLE_VALUE;
            }

            inline bool operator!=(LocalFdType fd,
                                   __FUCK_WINDOWS_INVALID_HANDLE_VALUE) noexcept {
                return fd != INVALID_HANDLE_VALUE;
            }

            inline bool operator!=(__FUCK_WINDOWS_INVALID_HANDLE_VALUE,
                                   LocalFdType fd) noexcept {
                return fd != INVALID_HANDLE_VALUE;
            }
        } // namespace internal

        inline constexpr auto kInvalidLocalFd 
            = internal::__FUCK_WINDOWS_INVALID_HANDLE_VALUE{};

        enum class OpenMode : ::DWORD {
            Read       = GENERIC_READ,
            Write      = GENERIC_WRITE,
            ReadWrite  = GENERIC_READ | GENERIC_WRITE,
            Append     = FILE_APPEND_DATA,
            Directory  = FILE_LIST_DIRECTORY
        };

        using ModeType = int;

        namespace internal {

            // --------------------- 编码转换 ---------------------
            inline std::wstring utf8ToUtf16(std::string_view utf8) {
                int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
                std::wstring wstr(len, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wstr.data(), len);
                return wstr;
            }

            // --------------------- 模式转换 ---------------------
            inline DWORD toWinAccess(OpenMode mode) {
                return static_cast<DWORD>(mode);
            }

            inline DWORD toWinDisposition(OpenMode mode) {
                switch (mode) {
                    case OpenMode::Read:
                    case OpenMode::Append:
                    case OpenMode::Directory:
                        return OPEN_EXISTING;
                    case OpenMode::Write:
                        return CREATE_ALWAYS;
                    case OpenMode::ReadWrite:
                        return OPEN_ALWAYS;
                    default:
                        return OPEN_EXISTING;
                }
            }

            inline DWORD toWinFlags(OpenMode mode) {
                DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_SYNCHRONOUS_IO_NONALERT;
                if (mode == OpenMode::Directory)
                    flags |= FILE_FLAG_BACKUP_SEMANTICS;
                return flags;
            }

            // --------------------- NtCreateFile 封装 ---------------------
            inline HANDLE openRelativeToDir(
                HANDLE dirHandle,
                std::wstring_view relativePath,
                DWORD desiredAccess,
                DWORD creationDisposition,
                DWORD fileFlags,
                ModeType /* mode 参数保留 */
            ) {
                UNICODE_STRING name;
                OBJECT_ATTRIBUTES attr;
                IO_STATUS_BLOCK ioStatus{};

                auto buffer = const_cast<wchar_t*>(relativePath.data());
                name.Length = static_cast<USHORT>(relativePath.size() * sizeof(wchar_t));
                name.MaximumLength = name.Length;
                name.Buffer = buffer;

                InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, dirHandle, nullptr);

                HANDLE resultHandle = nullptr;
                NTSTATUS status = ::NtCreateFile(
                    &resultHandle,
                    desiredAccess,
                    &attr,
                    &ioStatus,
                    nullptr,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    creationDisposition,
                    fileFlags,
                    nullptr,
                    0
                );

                if (!NT_SUCCESS(status)) {
                    ::SetLastError(::RtlNtStatusToDosError(status));
                    return INVALID_HANDLE_VALUE;
                }

                return resultHandle;
            }

        } // namespace internal

    } // namespace HX::platform
#else
    #error "Unsupported operating system"
#endif

#endif // !_HX_LOCAL_FD_API_H_