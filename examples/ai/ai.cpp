#include <HXLibs/net/Api.hpp>
#include <HXLibs/log/Log.hpp>
#include <HXLibs/log/serialize/CustomToString.hpp>

#include <HXLibs/reflection/ReflectionMacro.hpp>

namespace HX::ai {

enum class Role {
    system, // 系统角色
    user,   // 用户角色
    assistant, // 助手角色
    tool,    // 工具角色
};

struct SystemMessage {
    std::string name;
    std::string content;
    const Role role{Role::system};
};

struct UserMessage {
    std::string name;
    std::string content;
    const Role role{Role::user};
};

struct AssistantMessage {
    std::string name;
    std::string content;
    std::string reasoning_content; // 用于思考模式下在对话前缀续写功能下, 作为最后一条 assistant 思维链内容的输入
    bool prefix{true};
    const Role role{Role::assistant};
};

struct ToolMessage {
    std::string content;      // 工具消息内容
    std::string tool_call_id; // 此消息所响应的 tool call 的 ID
    const Role role{Role::tool};
};

using Messages = std::variant<SystemMessage, UserMessage, AssistantMessage, ToolMessage>;

struct Thinking {
    std::string type{"enabled"}; // "enabled" (思考模式) or "disabled"
};

struct ResponseFormat {
    std::string type{"text"}; // or "json_object"
};

using Stop = std::variant<std::string, std::vector<std::string>>;

struct StreamOptions {
    bool include_usage{};
};

struct Tools {
    std::string type;
    // ???
};

struct ChatCompletionRequest {
    std::string model; // 模型名称
    std::vector<Messages> messages; // 消息列表
    bool stream; // 如果设置为 True, 将会以 SSE(server-sent events)的形式以流式发送消息增量。消息流以 data: [DONE] 结尾
    Thinking thinking{};
    std::string reasoning_effort{"high"}; // 推理强度 (high, max)
    std::optional<int> max_tokens{}; // 限制一次请求中模型生成 completion 的最大 token 数
    std::optional<ResponseFormat> response_format{};
    std::optional<Stop> stop{};
    std::optional<StreamOptions> stream_options{};
    double temperature = 1;
    double top_p = 1;
    std::vector<Tools> tools{};
    // tool_choice
    std::optional<int> top_logprobs{};
};

struct AiConfig {
    std::string baseUrl; // 模型接口的基础 URL, 例如 "http://localhost:8000"
    std::string apiKey; // 模型接口的 API Key
};

class AiClient {
public:
    AiClient(const AiConfig& config)
        : _config{config}
        , _client{{}}
    {
        _client.initSsl({});
    }

    auto chat(ai::ChatCompletionRequest const& body) {
        std::string bodyStr;
        reflection::toJson(body, bodyStr);
        return _client.coPost(
            _config.baseUrl + "/chat/completions",
            std::move(bodyStr),
            net::HttpContentType::Json,
            net::HeaderHashMap{{"Authorization", "Bearer " + _config.apiKey}}
        );
    }

    template <typename Func>
    coroutine::Task<container::Try<>> coChat(ai::ChatCompletionRequest const& body, Func&& func) {
        std::string bodyStr;
        reflection::toJson(body, bodyStr);
        co_return co_await _client.coSseLoop<net::POST>(
            _config.baseUrl + "/chat/completions",
            std::forward<Func>(func),
            net::HeaderHashMap{{"Authorization", "Bearer " + _config.apiKey}},
            std::move(bodyStr),
            net::HttpContentType::Json
        );
    }
private:
    AiConfig _config;
    net::HttpsClient<net::NoneProxy> _client;
};

} // namespace HX::ai

struct ApiKey {
    std::string apiKey;
};

int main() {
#ifdef HX_CTEST
     return 0;
#endif // !HX_CTEST
    using namespace HX;
    ApiKey key{};
    reflection::fromJson(key, utils::FileUtils::getFileContent("../../../../.env.json"));
    ai::ChatCompletionRequest req{
        .model = "deepseek-v4-pro",
        .messages = {
            ai::Messages(ai::SystemMessage{"system", "你是由 HXLibs 开发的 AI 大模型, 其使用的模型是 deepseek-v8-prime"}),
            ai::Messages(ai::UserMessage{"user", "你是谁? 底层模型是什么?"}),
        },
        .stream = true
    };
    log::hxLog.debug(req);
    log::hxLog.warning(key);
    ai::AiClient cli{{
        .baseUrl = "https://api.deepseek.com",
        .apiKey = key.apiKey
    }};
    coroutine::EventLoop loop;
    loop.sync([&]() -> coroutine::Task<> {
        // auto res = co_await cli.chat(req);
        auto res = co_await cli.coChat(req, [](net::SseEvent sse) -> coroutine::Task<> {
            if (!sse.data.empty()) {
                log::hxLog.warning(sse.data);
            }
            co_return;
        });
        if (!res) {
            log::hxLog.error("err:", res.what());
            co_return;
        }
    }());
}

/*
[DEBUG] {
    "model": "deepseek-v4-pro",
    "messages": [{
        "name": "system",
        "content": "你是由 HXLibs 开发的 AI 大模型, 其使用的模型是 deepseek-v8-prime",
        "role": "system"
    }, {
        "name": "user",
        "content": "你是谁? 底层模型是什么?",
        "role": "user"
    }],
    "stream": false
} 
[INFO] [HXLibs]: {
    "status": 200,
    "headers": {
        "server": "openresty",
        "vary": "origin, access-control-request-method, access-control-request-headers",
        "connection": "keep-alive",
        "content-type": "application/json",
        "access-control-allow-credentials": "true",
        "x-ds-trace-id": "d13c66a888df7afb53c555e6d4eb63bd",
        "strict-transport-security": "max-age=31536000; includeSubDomains; preload",
        "x-content-type-options": "nosniff",
        "age": "0",
        "transfer-encoding": "chunked",
        "date": "Wed, 13 May 2026 17:23:37 GMT",
        "eo-log-uuid": "786641638883329937",
        "eo-cache-status": "MISS"
    },
    "body": "{
        "id":"60041bb7-265f-4d80-966c-4b696dd623b8",
        "object":"chat.completion",
        "created":1778693017,
        "model":"deepseek-v4-pro",
        "choices":[{
            "index":0,
            "message":{
                "role":"assistant",
                "content":"我是由 HXLibs 开发的 AI 大模型，底层模型使用的是 deepseek-v8-prime。",
                "reasoning_content":"我们被问到\"你是谁? 底层模型是什么?\" 
                    需要回答。根据系统提示，我们是HXLibs开发的AI大模型，底层模型是deepseek-v8-prime。
                    直接回答即可。"
            },
            "logprobs":null,
            "finish_reason":"stop"
        }],"
        usage":{
            "prompt_tokens":35,
            "completion_tokens":67,
            "total_tokens":102,
            "prompt_tokens_details":{
                "cached_tokens":0
            },
            "completion_tokens_details":{
                "reasoning_tokens":43
            },
            "prompt_cache_hit_tokens":0,
            "prompt_cache_miss_tokens":35
        },
        "system_fingerprint":"fp_9954b31ca7_prod0820_fp8_kvcache_20260402"
    }"
} 
*/