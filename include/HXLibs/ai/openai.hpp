#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2026-06-06 14:51:10
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

#include <string>

#include <HXLibs/net/client/HttpClientOptions.hpp>
#include <HXLibs/net/client/HttpClient.hpp>

namespace HX::ai::openai {

namespace role {

inline static constexpr auto& System{"system"};
inline static constexpr auto& User{"user"};
inline static constexpr auto& Assistant{"assistant"};
inline static constexpr auto& Tool{"tool"};

} // namespace role

template <typename Proxy>
struct AiConfig {
    std::string baseUrl; // 模型接口的基础 URL, 例如 "http://localhost:8000"
    std::string apiKey;  // 模型接口的 API Key
    net::HttpClientOptions<Proxy> clineOptions{};
};

struct Message {
    // 多模态 内容
    struct ContentPartText {
        std::string text;
        std::string type = "text";
    };

    struct ContentPartImage {
        struct ImageUrl {
            std::string url;
            std::string detail; // auto / low / high
        } image_url;
        std::string type = "image_url";
    };

    struct ToolCall {
        std::string id;
        struct Function {
            std::string name;
            std::string arguments; // json
        } function;
        std::string type = "function";
    };

    using ContentPart = std::variant<ContentPartText, ContentPartImage>;
    using MessageConent = std::variant<std::string, std::vector<ContentPart>>;

    std::string role; // 'system' | 'user' | 'assistant' | 'tool'
    MessageConent content;
    std::optional<std::string> name{};
    std::optional<std::vector<ToolCall>> tool_calls{};
    std::optional<std::string> tool_call_id{};
};

struct Tool {
    struct Function {
        std::string name;
        std::string description;
        std::string parameters; // json
    } function;
    std::string type = "function";
};

struct ToolChoiceFunction {
    std::string name;
};

struct ResponseFormat {
    std::string type{"text"}; // or "json_object"
};

struct ChatCompletionRequest {
    using ToolChoice = std::variant<std::string, ToolChoiceFunction>;

    // 必填
    std::string model; // 模型名称
    std::vector<Message> messages;

    // 可选
    bool stream{};
    std::optional<double> temperature{};
    std::optional<uint64_t> max_tokens{};
    std::optional<double> top_p{};
    std::optional<double> frequency_penalty{};
    std::optional<double> presence_penalty{};
    std::optional<std::vector<Tool>> tools{};
    std::optional<ToolChoice> tool_choice{}; // auto | none
    std::optional<ResponseFormat> response_format{};
};

struct ChatCompletionChunk {
    struct ToolCallDelta {
        std::size_t index{};
        std::optional<std::string> id;
        std::optional<std::string> type;
        struct Function {
            std::optional<std::string> name;
            std::optional<std::string> arguments;
        };
        std::optional<Function> function;
    };

    struct Delta {
        std::optional<std::string> role;
        std::optional<std::string> content;
        std::optional<std::vector<ToolCallDelta>> tool_calls;
    };

    struct Choice {
        std::size_t index{};
        Delta delta;
        std::optional<std::string> finish_reason; // 停止生成 token 的原因
    };
    
    std::string id;
    std::string object;
    uint64_t created{};
    std::string model;
    std::vector<Choice> choices;
};

template <template <class> typename CliType, typename Proxy = net::NoneProxy>
class OpenAiClient {
public:
    OpenAiClient(AiConfig<Proxy> cfg, std::shared_ptr<coroutine::EventLoop> loop = nullptr)
        : _cli{cfg.clineOptions, std::move(loop)}
        , _cfg{std::move(cfg)}
    {
        if constexpr (std::is_same_v<typename CliType<Proxy>::IoType, net::HttpsIO>) {
            _cli.initSsl({});
        }
    }

    auto chat(ChatCompletionRequest body) {
        std::string bodyStr;
        body.stream = false;
        reflection::toJson(body, bodyStr);
        return _cli.post(
            _cfg.baseUrl + "/chat/completions",
            std::move(bodyStr),
            net::HttpContentType::Json,
            net::HeaderHashMap{{"Authorization", "Bearer " + _cfg.apiKey}}
        );
    }

    template <typename Func>
        requires (std::same_as<std::invoke_result_t<Func, ChatCompletionChunk>, coroutine::Task<>>)
    coroutine::Task<container::Try<>> coChat(ChatCompletionRequest body, Func&& func) {
        std::string bodyStr;
        body.stream = true;
        reflection::toJson(body, bodyStr);
        co_return co_await _cli.template coSseLoop<net::POST>(
            _cfg.baseUrl + "/chat/completions",
            [&](net::SseEvent sse) -> coroutine::Task<> {
                if (sse.data == "[DONE]") [[unlikely]] {
                    co_return;
                }
                ChatCompletionChunk chunk;
                reflection::fromJson(chunk, sse.data);
                co_await std::forward<Func>(func)(chunk);
            },
            net::HeaderHashMap{{"Authorization", "Bearer " + _cfg.apiKey}},
            std::move(bodyStr),
            net::HttpContentType::Json
        );
    }

private:
    CliType<Proxy> _cli;
    AiConfig<Proxy> _cfg;
};

} // namespace HX::ai::openai
