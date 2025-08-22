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

/**
 * @brief 跨平台 本地 fd 类型定义
 * @note 只定义 local_fd_t, 不引入额外命名、宏或接口
 * @note 不要问我为什么, 你问一下win的架构师?
 */

#if defined(__linux__)
    #include <unistd.h>
    #include <fcntl.h>

#if IO_URING_DIRECT
    // O_DIRECT 可以用来减少操作系统内存复制的开销 (但需要注意可能的对齐要求)
    static constexpr int kOpenModeDefaultFlags = O_LARGEFILE | O_CLOEXEC | O_DIRECT;
#else
    // O_LARGEFILE 允许文件大小超过 2GB
    // O_CLOEXEC 确保在执行新程序时, 文件描述符不会继承到子进程
    static constexpr int kOpenModeDefaultFlags = O_LARGEFILE | O_CLOEXEC;
#endif

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
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>

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

        inline static constexpr int kReadFlag      = 1 << 1;
        inline static constexpr int kWriteFlag     = 1 << 2;
        inline static constexpr int kTruncFlag     = 1 << 3;
        inline static constexpr int kCreateFlag    = 1 << 4;
        inline static constexpr int kAppendFlag    = 1 << 5;
        inline static constexpr int kDirectoryFlag = 1 << 6;
        inline static constexpr int kReadWriteFlag = 1 << 7;
        
        enum class OpenMode : int {
            Read       = kReadFlag,
            Write      = kWriteFlag | kTruncFlag | kCreateFlag,
            ReadWrite  = kReadWriteFlag | kCreateFlag,
            Append     = kWriteFlag | kAppendFlag | kCreateFlag,
            Directory  = kReadFlag | kDirectoryFlag,
        };

        inline OpenMode operator|(OpenMode lhs, OpenMode rhs) {
            return static_cast<OpenMode>(static_cast<int>(lhs) | static_cast<int>(rhs));
        }

        struct Win32FileParams {
            DWORD access = 0;
            DWORD creation = 0;
            DWORD flags = FILE_ATTRIBUTE_NORMAL;

            inline static constexpr DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

            // 设置是否开启 IOCP (FILE_FLAG_OVERLAPPED)
            Win32FileParams& enableIocp(bool enable = true) {
                if (enable) {
                    flags |= FILE_FLAG_OVERLAPPED;
                } else {
                    flags &= ~FILE_FLAG_OVERLAPPED;
                }
                return *this;
            }
        };

        class Win32FileParamsBuilder {
            OpenMode mode_;
            bool iocp_ = false;
        public:
            explicit Win32FileParamsBuilder(OpenMode mode) : mode_(mode) {}

            Win32FileParamsBuilder& enableIocp(bool enable = true) {
                iocp_ = enable;
                return *this;
            }

            Win32FileParams build() const {
                Win32FileParams params{};
                params.flags = FILE_ATTRIBUTE_NORMAL;

                int m = static_cast<int>(mode_);

                if (m & kDirectoryFlag) {
                    params.access = GENERIC_READ;
                    params.creation = OPEN_EXISTING;
                    params.flags |= FILE_FLAG_BACKUP_SEMANTICS;
                } else if ((m & kReadWriteFlag) && (m & kCreateFlag)) {
                    params.access = GENERIC_READ | GENERIC_WRITE;
                    params.creation = OPEN_ALWAYS;
                } else if (m & kWriteFlag) {
                    if (m & kAppendFlag) {
                        params.access = FILE_APPEND_DATA;
                        params.creation = (m & kCreateFlag) ? OPEN_ALWAYS : OPEN_EXISTING;
                    } else if (m & kTruncFlag) {
                        params.access = GENERIC_WRITE;
                        params.creation = CREATE_ALWAYS;
                    } else {
                        params.access = GENERIC_WRITE;
                        params.creation = OPEN_EXISTING;
                    }
                } else if (m & kReadFlag) {
                    params.access = GENERIC_READ;
                    params.creation = OPEN_EXISTING;
                } else {
                    params.access = GENERIC_READ;
                    params.creation = OPEN_EXISTING;
                }

                if (iocp_) {
                    params.flags |= FILE_FLAG_OVERLAPPED;
                }

                return params;
            }
        };

        using ModeType = int;

    } // namespace HX::platform
#else
    #error "Unsupported operating system"
#endif

