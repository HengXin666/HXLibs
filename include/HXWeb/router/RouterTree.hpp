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
#ifndef _HX_ROUTER_TREE_H_
#define _HX_ROUTER_TREE_H_

#include <string>
#include <vector>
#include <stack>
#include <functional>

#include <HXWeb/protocol/http/Request.h>
#include <HXWeb/protocol/http/Response.h>
#include <HXSTL/coroutine/task/Task.hpp>
#include <HXSTL/container/RadixTree.hpp>

namespace HX { namespace web { namespace router {

using EndpointFunc = std::function<HX::STL::coroutine::task::Task<>(
    protocol::http::Request&, 
    protocol::http::Response&)>;

class RouterTree {
    using Node = HX::STL::container::RadixTreeNode<std::string_view, EndpointFunc>;
public:
    explicit RouterTree() 
        : _root(std::make_shared<Node>())
        , _notFoundHandler([](protocol::http::Request &req,
                              protocol::http::Response &res) 
        -> HX::STL::coroutine::task::Task<> {
            static_cast<void>(req);
            res.setResponseLine(protocol::http::Status::CODE_404)
               .setContentType(protocol::http::HTML)
               .setBodyData("<!DOCTYPE html><html><head><meta charset=UTF-8><title>404 Not Found</title><style>body{font-family:Arial,sans-serif;text-align:center;padding:50px;background-color:#f4f4f4}h1{font-size:100px;margin:0;color:#333}p{font-size:24px;color:#666}</style><body><h1>404</h1><p>Not Found</p><hr/><p>HXLibs</p>");
            co_return;
        })
    {}

    /**
     * @brief 设置`找不到路由`时候, 调用的端点
     * @param func 
     */
    void setNotFoundHandler(EndpointFunc&& func) {
        _notFoundHandler = std::move(func);
    }

    RouterTree& operator=(const RouterTree&) = delete;
    RouterTree(const RouterTree& ) = delete;

    void insert(
        std::vector<std::string_view>& buildLink,
        EndpointFunc&& endpoint
    ) {
        auto node = _root;
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

    const EndpointFunc& find(const std::vector<std::string_view>& findLink) const {
        const std::size_t n = findLink.size();
        std::stack<std::tuple<std::shared_ptr<Node>, std::size_t>> st;
        st.push({_root, static_cast<std::size_t>(0)});
        auto node = _root;
        while (st.size()) {
            std::size_t i = 0;
            auto& top = st.top();
            node = std::move(std::get<0>(top));
            i = std::get<1>(top);
            st.pop();
            auto findIt = node->child.end(); 
            if (i == n)
                goto End;

            for (; i < n; ++i) {
                findIt = node->child.find(findLink[i]);
                if (findIt == node->child.end()) {
                    // 以 {} 尝试
                    findIt = node->child.find("*");
                    if (findIt != node->child.end()) {
                        st.push({node, n});
                        node = findIt->second;
                        continue;
                    }

                End:
                    // 只能看看是否有**了
                    findIt = node->child.find("**");
                    if (findIt == node->child.end()) {
                        if (st.empty()) {
                            return _notFoundHandler;
                        }
                        break; // 回溯之前的
                    }
                    return *findIt->second->val;
                } else {
                    node = findIt->second;
                }
            }
        }
        return node->val.has_value() ? *node->val : _notFoundHandler;
    }

private:
    std::shared_ptr<Node> _root;

    /**
     * @brief 路由找不到时, 调用的端点; 俗称`404`
     */
    EndpointFunc _notFoundHandler;
};

}}} // namespace HX::web::router

#endif // !_HX_ROUTER_TREE_H_