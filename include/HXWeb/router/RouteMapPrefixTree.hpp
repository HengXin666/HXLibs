#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-27 21:37:34
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
#ifndef _HX_ROUTE_MAP_PREFIX_TREE_H_
#define _HX_ROUTE_MAP_PREFIX_TREE_H_

#include <HXSTL/container/RadixTree.hpp>

namespace HX { namespace web { namespace router {

/**
 * @brief 路由映射前缀树
 */
template <class T>
class RouteMapPrefixTree : public HX::STL::container::RadixTree<T> {

    std::optional<T> _find(
        const std::vector<std::string>& findLink,
        std::shared_ptr<HX::STL::container::RadixTreeNode<T>> node,
        std::size_t index
    ) const {
        std::size_t n = findLink.size();
        // 尝试{}开始的索引, 如果匹配不正确, 则回溯!
        for (std::size_t i = index + 1; i < n; ++i) {
            auto findIt = node->child.find(findLink[i]);
            if (findIt == node->child.end()) {
                // 以 {} 尝试
                findIt = node->child.find("*");
                if (findIt != node->child.end()) {
                    std::optional<T> res = _find(findLink, findIt->second, i);
                    if (res.has_value())
                        return res;
                }
                // 只能看看是否有**了
                findIt = node->child.find("**");
                if (findIt != node->child.end()) {
                    node = findIt->second;
                    break;
                }
               return std::nullopt;
            } else {
                node = findIt->second;
            }
        }
        return node->val;
    }

    using HX::STL::container::RadixTree<T>::_root; // 避免找不到父类成员
public:
    explicit RouteMapPrefixTree() 
        : HX::STL::container::RadixTree<T>()
    {}

    /**
     * @brief 构建字典树
     * @param buildLink 字典树构建链路, 如["home", "name", "{id}", "**"], 即 <root> -> home -> name -> * -> **
     * @param val 链路末端所具有的值
     * @warning 理论上必定插入成功, 只是可能会覆盖原来已有的!
     */
    void insert(const std::vector<std::string>& buildLink, const T& val) {
        auto node = _root;
        for (auto& it : buildLink) {
            std::string& key = const_cast<std::string&>(it);
            if (it[0] == '{') { // 特别处理: 如果是 {xxx}, 那么映射到 "*"
                key = "*";
            }
            auto findIt = node->child.find(key);
            if (findIt == node->child.end()) {
                node = node->child[key] = std::make_shared<HX::STL::container::RadixTreeNode<T>>();
            } else {
                node = findIt->second;
            }
        }
        node->val = val;
    }

    /**
     * @brief 从字典树中查找
     * @param findLink 查找链路, 如["home", "name", "{id}", "**"], 即 <root> -> home -> name -> * -> **
     * @return 查找结果的引用
     */
    std::optional<T> find(const std::vector<std::string>& findLink) const {
        auto node = _root;
        std::size_t n = findLink.size();
        // 尝试{}开始的索引, 如果匹配不正确, 则回溯!
        for (std::size_t i = 0; i < n; ++i) {
            auto findIt = node->child.find(findLink[i]);
            if (findIt == node->child.end()) {
                // 以 {} 尝试
                findIt = node->child.find("*");
                if (findIt != node->child.end()) {
                    std::optional<T> res = _find(findLink, findIt->second, i);
                    if (res.has_value())
                        return res;
                }

                // 只能看看是否有**了
                findIt = node->child.find("**");
                if (findIt != node->child.end()) {
                    node = findIt->second;
                    break;
                }
               return std::nullopt;
            } else {
                node = findIt->second;
            }
        }
        return node->val;
    }
};

}}} // namespace HX::web::router

#endif // _HX_ROUTE_MAP_PREFIX_TREE_H_
