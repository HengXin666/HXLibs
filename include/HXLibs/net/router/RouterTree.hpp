#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-01-22 21:17:18
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

#include <vector>
#include <functional>

#include <HXLibs/net/protocol/http/Request.hpp>
#include <HXLibs/net/protocol/http/Response.hpp>
#include <HXLibs/coroutine/task/Task.hpp>
#include <HXLibs/container/RadixTree.hpp>

namespace HX::net {

template <typename IOType>
using EndpointFunc = std::function<coroutine::Task<>(HttpRequest<IOType>&, HttpResponse<IOType>&)>;

template <typename IOType>
class RouterTree {
    using EndpointType = EndpointFunc<IOType>;
    using Node = container::RadixTreeNode<std::string_view, EndpointType>;
public:
    explicit RouterTree() 
        : _root(std::make_shared<Node>())
        , _notFoundHandler([](HttpRequest<IOType> &req,
                              HttpResponse<IOType> &res) 
        -> coroutine::Task<> {
            static_cast<void>(req);
            co_return co_await res
                .setResLine(Status::CODE_404)
                .setContentType(HTML)
                .setBody(
    "<!DOCTYPE html><html lang=\"en\">"
    "<head><meta charset=\"UTF-8\"/>"
    "<title>404 Not Found</title>"
    "<style>"
        "body{font-family:Arial,sans-serif;text-align:center;padding:50px;background-color:#f4f4f4;}"
        "h1{font-size:100px;margin:0;color:#333;}"
        "p{font-size:24px;color:#666;}"
    "</style></head>"
    "<body>"
        "<h1>404</h1>"
        "<p>Not Found</p>"
        "<hr/>"
        "<p>HXLibs</p>"
    "</body></html>")
                .sendRes();
        })
    {}

    /**
     * @brief 设置`找不到路由`时候, 调用的端点
     * @param func 
     */
    void setNotFoundHandler(EndpointType&& func) {
        _notFoundHandler = std::move(func);
    }

    RouterTree& operator=(const RouterTree&) = delete;
    RouterTree(const RouterTree& ) = delete;

    void insert(
        std::vector<std::string_view>& buildLink,
        EndpointType&& endpoint
    ) {
        auto node = _root;
        // @todo, 应该校验是否合法, 比如 /xxx{id}/xxx 显然不支持. 以及 /*** 这种.
        // @todo, 应该支持或者明确不支持中文路径, 因为需要转义
        for (auto& key : buildLink) {
            if (key.front() == '{') { // 特别处理: 如果是 {xxx}, 那么映射到 "*"
                key = "*";
            }
            auto findIt = node->child.find(key);
            if (findIt == node->child.end()) {
                node = node->child[key] = std::make_shared<Node>();
            } else {
                node = findIt->second;
            }
        }
        node->val = endpoint;
    }

    const EndpointType& find(std::span<std::string_view const> findLink) const {
        auto const* opt = _find(findLink, _root);
        if (opt) {
            return *opt;
        }
        return _notFoundHandler;
    }

private:
    EndpointType const* _find(
        std::span<std::string_view const> findLink,
        std::shared_ptr<Node> const& node
    ) const {
        if (findLink.empty()) {
            return node->val ? &(*node->val) : nullptr;
        }
        using namespace std::string_view_literals;
        std::array<std::string_view const, 2> arr{findLink.front(), "*"sv};
        for (auto const findStr : arr) {
            auto it = node->child.find(findStr);
            if (it != node->child.end()) {
                if (auto* res = _find(findLink.subspan(1), it->second)) {
                    return res;
                }
            }
        }
        // 处理通配符
        auto it = node->child.find("**");
        if (it != node->child.end()) {
            return _find({}, it->second);
        }
        return nullptr;
    }

    std::shared_ptr<Node> _root;

    /**
     * @brief 路由找不到时, 调用的端点; 俗称`404`
     */
    EndpointType _notFoundHandler;
};

} // namespace HX::net

