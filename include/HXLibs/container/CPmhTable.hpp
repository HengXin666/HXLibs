#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-18 15:44:02
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

#include <cstdint>

#include <HXLibs/container/CPmhBucket.hpp>

namespace HX::container {

namespace internal {

struct SeedOrIndex {
    using value_type = uint64_t;
private:
    inline static constexpr value_type HighMask = 1ULL << (sizeof(value_type) * 8 - 1);

    // 高位为 1 则是 种子, 反之则是索引
    // 注意: 高位只是标记, 不影响实际的值
    // 特别的, 种子应该为 uint32_t 类型, 负责可能出错
    value_type _val = 0;
public:
    constexpr SeedOrIndex() = default;

    constexpr SeedOrIndex(bool isSeed, std::size_t seedOrIndex)
        : _val{seedOrIndex | (isSeed * HighMask)}
    {}

    constexpr bool isSeed() const noexcept {
        return _val & HighMask /* >> (sizeof(value_type) * 8 - 1) */; // 默认有值就是 true
    }

    constexpr value_type getSeed() const noexcept {
        return _val ^ HighMask; // 去掉这个用于标记的最高位
    }

    constexpr value_type getIndex() const noexcept {
        return _val;
    }
};

} // namespace internal

template <std::size_t M, typename Hasher>
struct CPmhTable {
    // 根种子
    std::size_t _rootSeed;

    // 一级哈希表
    std::array<internal::SeedOrIndex, M> _g;

    // 二级哈希表
    std::array<std::size_t, M> _h;

    Hasher _hash;

    constexpr CPmhTable(
        std::size_t rootSeed,
        std::array<internal::SeedOrIndex, M> G,
        std::array<std::size_t, M> H,
        Hasher hash
    )
        : _rootSeed{rootSeed}
        , _g{G}
        , _h{H}
        , _hash{hash}
    {}

    template <typename Key>
    constexpr std::size_t lookup(Key const& key) const {
        return lookup(key, _hash);
    }

    template <typename Key, typename HashType>
    constexpr std::size_t lookup(Key const& key, HashType const& hash) const {
        auto const& seedOrIdx = _g[hash(key, _rootSeed) % M];
        if (seedOrIdx.isSeed()) {
            return _h[hash(key, static_cast<std::size_t>(seedOrIdx.getSeed())) % M];
        } else {
            return static_cast<std::size_t>(seedOrIdx.getIndex());
        }
    }
};

template <std::size_t M, typename Item, std::size_t N, typename Hasher, typename PRG>
constexpr CPmhTable<M, Hasher> makeCPmhTable(std::array<Item, N> items, Hasher const& hash, PRG prg) {
    // 1. 创建桶数组, 并且确定 [元素键 -> 桶] 的哈希种子
    auto bucketArr = makeCPmhBucket<M>(items, hash, prg);

    // 2. 按照桶元素从大到小, 排序桶数组, 得到桶引用数组
    auto bucketRefArr = bucketArr.sortBucketRefArr();

    // 3. 创建2个哈希表
    std::array<internal::SeedOrIndex, M> G;
    std::array<std::size_t, M> H;

    constexpr std::size_t Illegal = static_cast<std::size_t>(-1); // 哨兵, 表示空位
    H.fill(Illegal);

    // 4. 不断尝试合法选择
    for (auto const& bucket : bucketRefArr) {
        auto const bucketSize = bucket.size();
        if (bucketSize == 1) {
            // 直接就是索引
            G[bucket.idx] = {false, bucket[0]};
        } else if (bucketSize > 1) {
            // 临时保存的结果, 方便直接反悔
            CVector<std::size_t, decltype(bucketArr)::BucketMax> tmp;
            bool isNext = true;
            while (true) {
                std::size_t seed = prg();
                isNext = false;
                for (std::size_t i = 0; i < bucketSize; ++i) {
                    std::size_t idx = hash(meta::getKey(items[bucket[i]]), seed) % M;
                    // 如果没有在之前的映射中出现就是合法的, 否则重新选择哈希函数
                    if (H[idx] != Illegal || tmp.find(idx) != tmp.end()) {
                        tmp.clear();
                        isNext = true;
                        break;
                    }
                    tmp.push_back(idx);
                }
                if (!isNext) {
                    G[bucket.idx] = {true, seed};
                    for (std::size_t i = 0; i < bucketSize; ++i) {
                        H[tmp[i]] = bucket[i];
                    }
                    break;
                }
            }
        }
    }

    // 5. 返回
    return {bucketArr.seed, G, H, hash};
}

} // namespace HX::container

