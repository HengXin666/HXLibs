#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-09-08 14:31:47
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

#include <mutex>

#include <HXLibs/net/client/HttpClient.hpp>

namespace HX::net {

template <typename Timeout, typename Proxy>
    requires(utils::HasTimeNTTP<Timeout>)
class HttpClientPool {
public:
    using HttpClientType = HttpClient<Timeout, Proxy>;

    HttpClientPool(std::size_t size, HttpClientOptions<Timeout, Proxy> options = HttpClientOptions{})
        : _cliPool{}
    {
        if (size <= 0) [[unlikely]] {
            throw std::runtime_error{"HttpClientPool: Size must be at least 1"};
        }
        for (std::size_t i = 0; i < size; ++i) {
            _cliPool.emplace_back(std::make_unique<HttpClientType>(options));
        }
    }

    HttpClientPool& operator=(HttpClientPool&&) noexcept = delete;

    void resize(std::size_t newSize, HttpClientOptions<Timeout, Proxy> options = HttpClientOptions{}) {
        if (newSize <= 0) [[unlikely]] {
            throw std::runtime_error{"HttpClientPool: Size must be at least 1"};
        }
        std::unique_lock _{_mtx};
        if (newSize <= _cliPool.size()) {
            _cliPool.resize(newSize);
        } else {
            for (std::size_t i = _cliPool.size(); i <= newSize; ++i) {
                _cliPool.emplace_back(std::make_unique<HttpClientType>(options));
            }
        }
    }

    /**
     * @brief 获取索引并且推进到下一个索引
     * @return std::size_t 
     */
    std::size_t getIdxAndNext() noexcept {
        std::unique_lock _{_mtx};
        return ++_index %= _cliPool.size();
    }

    /**
     * @brief 发送 Get 请求
     * @param url 
     * @param headers 
     * @return container::FutureResult<ResponseData> 
     */
    container::FutureResult<container::Try<ResponseData>> get(
        std::string url,
        HeaderHashMap headers = {}
    ) {
        return _cliPool.at(getIdxAndNext())->get(
            std::move(url), std::move(headers)
        );
    }

    /**
     * @brief 发送一个 POST 请求
     * @param url 请求的 URL
     * @param body 请求正文
     * @param contentType 请求正文类型
     * @return container::FutureResult<container::Try<ResponseData>> 
     */
    container::FutureResult<container::Try<ResponseData>> post(
        std::string url,
        std::string body,
        HttpContentType contentType,
        HeaderHashMap headers = {}
    ) {
        return _cliPool.at(getIdxAndNext())->post(
            std::move(url), std::move(body),
            contentType, std::move(headers)
        );
    }

    /**
     * @brief 异步的发送一个请求
     * @tparam Method 请求类型
     * @tparam Str 正文字符串类型
     * @param url url 或者 path (以连接的情况下)
     * @param body 正文
     * @param contentType 正文类型 
     * @return container::FutureResult<container::Try<ResponseData>> 响应数据
     */
    template <HttpMethod Method, meta::StringType Str = std::string>
    container::FutureResult<container::Try<ResponseData>> requst(
        std::string url,
        HeaderHashMap headers = {},
        Str&& body = {},
        HttpContentType contentType = HttpContentType::None
    ) {
        return _cliPool.at(getIdxAndNext())->template requst<Method>(
            std::move(url),
            std::move(headers),
            std::forward<Str>(body),
            contentType
        );
    }

    /**
     * @brief 创建一个 WebSocket 循环, 其以线程池的方式独立运行
     * @tparam Func 
     * @param url  ws 的 url, 如 ws://127.0.0.1:28205/ws (如果不对则抛异常)
     * @param func 该声明为 [](WebSocketClient ws) -> coroutine::Task<> { }
     * @return container::FutureResult<container::Try<Res>>
     */
    template <
        typename Func, 
        typename Res = coroutine::AwaiterReturnValue<std::invoke_result_t<Func, WebSocketClient>>
    >
        requires(std::is_same_v<std::invoke_result_t<Func, WebSocketClient>, coroutine::Task<Res>>)
    container::FutureResult<container::Try<Res>> wsLoop(std::string url, Func&& func) {
        return _cliPool.at(getIdxAndNext())->wsLoop(
            std::move(url), std::forward<Func>(func)
        );
    }

    /**
     * @brief 分块编码上传文件
     * @tparam Method 请求方式
     * @param url 请求 URL
     * @param path 需要上传的文件路径
     * @param contentType 正文类型
     * @param headers 请求头
     * @return container::FutureResult<>
     */
    template <HttpMethod Method>
    container::FutureResult<ResponseData> uploadChunked(
        std::string url,
        std::string path,
        HttpContentType contentType = HttpContentType::Text,
        HeaderHashMap headers = {}
    ) {
        return _cliPool.at(getIdxAndNext())->uploadChunked(
            std::move(url), std::move(path),
            contentType, std::move(headers)
        );
    }

    /**
     * @brief 断开连接
     * @return container::FutureResult<> 
     */
    container::FutureResult<> close() {
        return _cliPool.at(getIdxAndNext())->close();
    }
private:
    std::vector<std::unique_ptr<HttpClientType>> _cliPool;
    std::mutex _mtx;
    std::uint64_t _index;
};

HttpClientPool(std::size_t size) -> HttpClientPool<decltype(utils::operator""_ms<"5000">()), Socks5Proxy>;

} // namespace HX::net