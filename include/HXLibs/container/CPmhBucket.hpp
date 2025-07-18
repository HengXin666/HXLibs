#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-18 17:26:52
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
#ifndef _HX_C_PMH_BUCKET_H_
#define _HX_C_PMH_BUCKET_H_

#include <algorithm>

#include <HXLibs/container/CVector.hpp>
#include <HXLibs/meta/Hash.hpp>

namespace HX::container {

namespace internal {

template <typename T>
constexpr T log(T v) {
    T res = 0;
    while (v > 0) {
        ++res;
        v >>= 1;
    }
    return res;
}

} // namespace internal

template <std::size_t M>
struct CPmhBucket {
    // 桶容量
    inline static constexpr std::size_t BucketMax = 2 * (1 << internal::log(M) / 2);

    using BucketType = CVector<std::size_t, BucketMax>;

    std::array<BucketType, M> bucketArr; // 桶数组

    std::size_t seed; // 哈希种子

    constexpr CPmhBucket() = default;

    // 桶引用, 这样就只需要排序引用就可以了
    // 因为直接排序桶数组的开销很大 (即便使用 move, T[N] 也只能一个一个来啊..., 是O(N))
    struct BucketRef {
        std::size_t idx; // 桶数组索引
        BucketType const* ptr; // 桶引用 (采用指针, 因为引用不能拷贝)

        constexpr std::size_t operator[](std::size_t idx) const noexcept {
            return (*ptr)[idx];
        }

        constexpr std::size_t size() const noexcept {
            return ptr->size();
        }
    };

    template <std::size_t... Idx>
    constexpr std::array<BucketRef, M> makeBucketRefArr(std::index_sequence<Idx...>) const {
        return {BucketRef{Idx, &bucketArr[Idx]}...};
    }

    constexpr auto sortBucketRefArr() const {
        auto res = makeBucketRefArr(std::make_index_sequence<M>{});
        std::sort(res.begin(), res.end(), [](BucketRef const& b1, BucketRef const& b2) {
            return b1.size() > b2.size();
        });
        return res;
    }

    constexpr void clear() noexcept {
        for (auto& bucket : bucketArr) {
            bucket.clear();
        }
    }
};

template <std::size_t M, std::size_t N, typename Item, typename Hasher, typename PRG>
constexpr CPmhBucket<M> makeCPmhBucket(
    std::array<Item, N> const& items,
    Hasher const& hash,
    PRG& prg
) {
    CPmhBucket<M> res{};
    bool isNext = true; // 是否继续寻找种子
    while (isNext) {
        res.clear();
        res.seed = prg(); // 放大是合法的 uint32_t -> size_t
        isNext = false;
        for (std::size_t i = 0; i < N; ++i) {
            auto& bucket = res.bucketArr[hash(meta::getKey(items[i]), res.seed) % M];
            if (bucket.size() >= CPmhBucket<M>::BucketMax) {
                isNext = true;
                break;
            }
            bucket.push_back(i);
        }
    }
    return res;
}

} // namespace HX::container

#endif // !_HX_C_PMH_BUCKET_H_