#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-31 14:36:36
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
 * */

#include <fstream>
#include <string>
#include <string_view>
#include <filesystem>
#include <span>

#include <HXLibs/platform/LocalFdApi.hpp>

#include <HXLibs/coroutine/task/Task.hpp>
#include <HXLibs/coroutine/loop/EventLoop.hpp>

#include <HXLibs/log/Log.hpp> // debug

/**
 * @brief @todo !!!本类需要大重构!!!
 */

namespace HX::utils {

using platform::LocalFdType;
using platform::kInvalidLocalFd;
using platform::OpenMode;
using platform::ModeType;

/**
 * @brief 文件操作类
 */
struct FileUtils {
    /// @brief 读取文件buf数组的缓冲区大小
    inline static constexpr std::size_t kBufMaxSize = 1 << 17; // 128KB
public:
    /**
     * @brief 获取文件拓展名 (如`loli.png`->`.png`)
     * @param name 文件名
     * @return std::string_view 
     */
    inline static std::string_view getExtension(std::string_view name) {
        size_t pos = name.rfind('.');
        if (pos == std::string_view::npos) {
            return {};
        }
        return name.substr(pos);
    }

    /**
     * @brief 获取文件的大小
     * @param filePath 文件路径
     * @return std::uintmax_t 大小 (单位: 字节)
     */
    inline static std::uintmax_t getFileSize(std::string_view filePath) {
        return std::filesystem::file_size(
            std::filesystem::path{filePath}
        );
    }

    /**
     * @brief [同步的]读取文件内容
     * @param path 文件路径
     * @return std::string 文件内容
     */
    static std::string getFileContent(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) [[unlikely]] {
            throw std::system_error{errno, std::generic_category()};
        }
        return std::string{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };
    }

    /**
     * @brief [同步的]向文件写入数据
     * @param path 文件路径
     * @param content 需要写入的数据
     */
    static void putFileContent(const std::string& path, std::string_view content) {
        std::ofstream file(path);
        if (!file.is_open()) [[unlikely]] {
            throw std::system_error{errno, std::generic_category()};
        }
        std::copy(
            content.begin(),
            content.end(),
            std::ostreambuf_iterator<char>(file)
        );
    }
};

/**
 * @brief 异步文件类
 * @note 支持同步操作, 但是同步操作不应该出现在 co_await 协程语义上下文中
 */
class AsyncFile {
public:
    AsyncFile(coroutine::EventLoop& eventLoop)
        : _eventLoop{eventLoop}
        , _offset{}
        , _fd{kInvalidLocalFd}
    {}

    /**
     * @brief 异步打开文件
     * @param path 文件路径
     * @param flags 打开方式: OpenMode (枚举 如: OpenMode::Write | OpenMode::Append)
     * @param mode 文件权限模式, 仅在文件创建时有效 (一般写0644)
     * @param dirfd 目录文件描述符, 它表示相对路径的基目录; `AT_FDCWD`, 则表示相对于当前工作目录
     * @return coroutine::Task<> 
     */
    coroutine::Task<> open(
        std::string_view path,
        OpenMode flags = OpenMode::ReadWrite,
        int dirfd = AT_FDCWD,
        ModeType mode = 0644
    ) {
#if defined(__linux__)
        _fd = HXLIBS_CHECK_EVENT_LOOP(
            co_await _eventLoop.makeAioTask().prepOpenat(
                dirfd, path.data(), static_cast<int>(flags), mode
            )
        );
#elif defined(_WIN32)
        auto params =
            platform::Win32FileParamsBuilder(flags).enableIocp(true)
                                                   .build();

        HANDLE fileHandle = ::CreateFileA(
            path.data(),                            // 文件名
            params.access,                          // 读写权限
            platform::Win32FileParams::shareMode,   // 共享模式
            nullptr,                                // 默认安全属性
            params.creation,                        // 如果文件不存在则创建
            params.flags,                           // 必须带 FILE_FLAG_OVERLAPPED
            nullptr                                 // 不拷贝句柄
        );

        if (fileHandle == INVALID_HANDLE_VALUE) [[unlikely]] {
            throw std::system_error(
                GetLastError(),
                std::system_category(),
                "CreateFileW failed"
            );
        }

        _fd = reinterpret_cast<LocalFdType>(fileHandle);
        co_return;
#else
        #error "Unsupported platform"
#endif
    }

    /**
     * @brief 同步打开文件
     * @param path 文件路径
     * @param flags 打开方式: OpenMode (枚举 如: OpenMode::Write | OpenMode::Append)
     * @param mode 文件权限模式, 仅在文件创建时有效 (一般写0644)
     * @param dirfd 目录文件描述符, 它表示相对路径的基目录; `AT_FDCWD`, 则表示相对于当前工作目录
     * @return void
     */
    void syncOpen(
        std::string_view path,
        OpenMode flags = OpenMode::ReadWrite,
        int dirfd = AT_FDCWD,
        ModeType mode = 0644
    ) {
        _eventLoop.sync(open(path, flags, dirfd, mode));
    }

    /**
     * @brief 异步读取文件内容到 buf
     * @param buf [out] 读取到的数据
     * @return int 读取的字节数
     */
    coroutine::Task<int> read(std::span<char> buf) {
        int len = static_cast<int>(HXLIBS_CHECK_EVENT_LOOP(
            co_await _eventLoop.makeAioTask().prepRead(
                _fd, buf, _offset
            )
        ));
        _offset += static_cast<uint64_t>(len);
        co_return len;
    }

    /**
     * @brief 异步读取文件内容到 buf
     * @param buf [out] 读取到的数据
     * @param size 读取的长度
     * @return int 读取的字节数
     */
    coroutine::Task<int> read(std::span<char> buf, unsigned int size) {
        int len = static_cast<int>(HXLIBS_CHECK_EVENT_LOOP(
            co_await _eventLoop.makeAioTask().prepRead(
                _fd, buf, size, _offset
            )
        ));
        _offset += static_cast<uint64_t>(len);
        co_return len;
    }

    coroutine::Task<std::string> readAll() {
        std::string res;
        std::vector<char> buf;
        buf.resize(FileUtils::kBufMaxSize);
        for (int len = 0; (len = co_await read(buf)); ) {
            res += std::string_view{buf.data(), static_cast<std::size_t>(len)};
        }
        co_return res;
    }

    /**
     * @brief 同步读取文件内容到 buf
     * @param buf [out] 读取到的数据
     * @return int 读取的字节数
     */
    int syncRead(std::span<char> buf) {
        return _eventLoop.sync(read(buf));
    }

    /**
     * @brief 同步的读取全部内容到 buf
     * @return std::string 
     */
    std::string syncReadAll() {
        return _eventLoop.sync(readAll());
    }

    /**
     * @brief 同步读取文件内容到 buf
     * @param buf [out] 读取到的数据
     * @param size 读取的长度
     * @return int 读取的字节数
     */
    int syncRead(std::span<char> buf, unsigned int size) {
        return _eventLoop.sync(read(buf, size));
    }

    /**
     * @brief 异步将 buf 写入到文件中, 内部保证完全写入
     * @param buf [in] 需要写入的数据
     */
    coroutine::Task<> write(std::span<char const> buf) {
        for (; !buf.empty(); buf = buf.subspan(
            static_cast<std::size_t>(HXLIBS_CHECK_EVENT_LOOP(
                co_await _eventLoop.makeAioTask().prepWrite(
                    _fd, buf, static_cast<std::uint64_t>(-1)
                )
            ))
        )) {
            // 文件 大多数情况下内核会尽量写满, 部分写很少见 (但不能依赖这一点!)
        }
    }

    /**
     * @brief 同步将 buf 写入到文件中, 内部保证完全写入
     * @param buf [in] 需要写入的数据
     */
    void syncWrite(std::span<char> buf) {
        _eventLoop.sync(write(buf));
    }

    /**
     * @brief 异步关闭文件
     * @return coroutine::Task<> 
     */
    coroutine::Task<> close() {
        HXLIBS_CHECK_EVENT_LOOP(
            co_await _eventLoop.makeAioTask().prepClose(_fd)
        );
        _fd = kInvalidLocalFd;
    }

    /**
     * @brief 同步关闭文件
     */
    void syncClose() {
        _eventLoop.sync(close());
    }

    /**
     * @brief 设置偏移量
     * @param offset 
     */
    void setOffset(uint64_t offset) {
        _offset = offset;
    }

#ifdef NDEBUG
    ~AsyncFile() noexcept = default;
#else
    ~AsyncFile() noexcept(false) {
        if (_fd != kInvalidLocalFd) [[unlikely]] {
            throw std::runtime_error{"[AsyncFile]: Before that, it is necessary to call close()"};
        }
    }
#endif // !NDEBUG

    AsyncFile& operator=(AsyncFile&&) noexcept = delete;

private:
    coroutine::EventLoop& _eventLoop;
    uint64_t _offset;
    LocalFdType _fd;
};

/**
 * @brief OpenMode 位运算符重载 (目前有意义的只有 Read | Append (r+))
 * @warning 几乎用不到这个! 如果使用了请看看`OpenMode`是什么东西先!
 */
constexpr OpenMode operator|(OpenMode lhs, OpenMode rhs) {
    return static_cast<OpenMode>(
        // static_cast<int>(lhs) |
        // static_cast<int>(rhs)
        static_cast<std::underlying_type_t<OpenMode>>(lhs) |
        static_cast<std::underlying_type_t<OpenMode>>(rhs)
    );
}

} // namespace HX::utils
