#pragma once
/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2025-07-20 17:53:28
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
#ifndef _HX_SHA1_H_
#define _HX_SHA1_H_

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace HX::net {

// Based on https://github.com/vog/sha1
/*
    Original authors:
    Steve Reid (Original C Code)
    Bruce Guenter (Small changes to fit into bglibs)
    Volker Grabsch (Translation to simpler C++ Code)
    Eugene Hopkinson (Safety improvements)
    Vincent Falco (beast adaptation)
*/
// 代码来源: https://github.com/alibaba/yalantinglibs/blob/main/include/ylt/standalone/cinatra/sha1.hpp

namespace sha1 {

static std::size_t constexpr BLOCK_INTS = 16;
static std::size_t constexpr BLOCK_BYTES = 64;
static std::size_t constexpr DIGEST_BYTES = 20;

inline std::uint32_t rol(std::uint32_t value, std::size_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

inline std::uint32_t blk(std::uint32_t block[BLOCK_INTS], std::size_t i) {
    return rol(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ block[(i + 2) & 15]
                   ^ block[i],
               1);
}

inline void R0(std::uint32_t block[BLOCK_INTS], std::uint32_t v,
               std::uint32_t& w, std::uint32_t x, std::uint32_t y,
               std::uint32_t& z, std::size_t i) {
    z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5);
    w = rol(w, 30);
}

inline void R1(std::uint32_t block[BLOCK_INTS], std::uint32_t v,
               std::uint32_t& w, std::uint32_t x, std::uint32_t y,
               std::uint32_t& z, std::size_t i) {
    block[i] = blk(block, i);
    z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5);
    w = rol(w, 30);
}

inline void R2(std::uint32_t block[BLOCK_INTS], std::uint32_t v,
               std::uint32_t& w, std::uint32_t x, std::uint32_t y,
               std::uint32_t& z, std::size_t i) {
    block[i] = blk(block, i);
    z += (w ^ x ^ y) + block[i] + 0x6ed9eba1 + rol(v, 5);
    w = rol(w, 30);
}

inline void R3(std::uint32_t block[BLOCK_INTS], std::uint32_t v,
               std::uint32_t& w, std::uint32_t x, std::uint32_t y,
               std::uint32_t& z, std::size_t i) {
    block[i] = blk(block, i);
    z += (((w | x) & y) | (w & x)) + block[i] + 0x8f1bbcdc + rol(v, 5);
    w = rol(w, 30);
}

inline void R4(std::uint32_t block[BLOCK_INTS], std::uint32_t v,
               std::uint32_t& w, std::uint32_t x, std::uint32_t y,
               std::uint32_t& z, std::size_t i) {
    block[i] = blk(block, i);
    z += (w ^ x ^ y) + block[i] + 0xca62c1d6 + rol(v, 5);
    w = rol(w, 30);
}

inline void make_block(std::uint8_t const* p, std::uint32_t block[BLOCK_INTS]) {
    for (std::size_t i = 0; i < BLOCK_INTS; i++)
        block[i] = (static_cast<std::uint32_t>(p[4 * i + 3]))
                   | (static_cast<std::uint32_t>(p[4 * i + 2])) << 8
                   | (static_cast<std::uint32_t>(p[4 * i + 1])) << 16
                   | (static_cast<std::uint32_t>(p[4 * i + 0])) << 24;
}

template <class = void>
void transform(std::uint32_t digest[], std::uint32_t block[BLOCK_INTS]) {
    std::uint32_t a = digest[0];
    std::uint32_t b = digest[1];
    std::uint32_t c = digest[2];
    std::uint32_t d = digest[3];
    std::uint32_t e = digest[4];

    R0(block, a, b, c, d, e, 0);
    R0(block, e, a, b, c, d, 1);
    R0(block, d, e, a, b, c, 2);
    R0(block, c, d, e, a, b, 3);
    R0(block, b, c, d, e, a, 4);
    R0(block, a, b, c, d, e, 5);
    R0(block, e, a, b, c, d, 6);
    R0(block, d, e, a, b, c, 7);
    R0(block, c, d, e, a, b, 8);
    R0(block, b, c, d, e, a, 9);
    R0(block, a, b, c, d, e, 10);
    R0(block, e, a, b, c, d, 11);
    R0(block, d, e, a, b, c, 12);
    R0(block, c, d, e, a, b, 13);
    R0(block, b, c, d, e, a, 14);
    R0(block, a, b, c, d, e, 15);
    R1(block, e, a, b, c, d, 0);
    R1(block, d, e, a, b, c, 1);
    R1(block, c, d, e, a, b, 2);
    R1(block, b, c, d, e, a, 3);
    R2(block, a, b, c, d, e, 4);
    R2(block, e, a, b, c, d, 5);
    R2(block, d, e, a, b, c, 6);
    R2(block, c, d, e, a, b, 7);
    R2(block, b, c, d, e, a, 8);
    R2(block, a, b, c, d, e, 9);
    R2(block, e, a, b, c, d, 10);
    R2(block, d, e, a, b, c, 11);
    R2(block, c, d, e, a, b, 12);
    R2(block, b, c, d, e, a, 13);
    R2(block, a, b, c, d, e, 14);
    R2(block, e, a, b, c, d, 15);
    R2(block, d, e, a, b, c, 0);
    R2(block, c, d, e, a, b, 1);
    R2(block, b, c, d, e, a, 2);
    R2(block, a, b, c, d, e, 3);
    R2(block, e, a, b, c, d, 4);
    R2(block, d, e, a, b, c, 5);
    R2(block, c, d, e, a, b, 6);
    R2(block, b, c, d, e, a, 7);
    R3(block, a, b, c, d, e, 8);
    R3(block, e, a, b, c, d, 9);
    R3(block, d, e, a, b, c, 10);
    R3(block, c, d, e, a, b, 11);
    R3(block, b, c, d, e, a, 12);
    R3(block, a, b, c, d, e, 13);
    R3(block, e, a, b, c, d, 14);
    R3(block, d, e, a, b, c, 15);
    R3(block, c, d, e, a, b, 0);
    R3(block, b, c, d, e, a, 1);
    R3(block, a, b, c, d, e, 2);
    R3(block, e, a, b, c, d, 3);
    R3(block, d, e, a, b, c, 4);
    R3(block, c, d, e, a, b, 5);
    R3(block, b, c, d, e, a, 6);
    R3(block, a, b, c, d, e, 7);
    R3(block, e, a, b, c, d, 8);
    R3(block, d, e, a, b, c, 9);
    R3(block, c, d, e, a, b, 10);
    R3(block, b, c, d, e, a, 11);
    R4(block, a, b, c, d, e, 12);
    R4(block, e, a, b, c, d, 13);
    R4(block, d, e, a, b, c, 14);
    R4(block, c, d, e, a, b, 15);
    R4(block, b, c, d, e, a, 0);
    R4(block, a, b, c, d, e, 1);
    R4(block, e, a, b, c, d, 2);
    R4(block, d, e, a, b, c, 3);
    R4(block, c, d, e, a, b, 4);
    R4(block, b, c, d, e, a, 5);
    R4(block, a, b, c, d, e, 6);
    R4(block, e, a, b, c, d, 7);
    R4(block, d, e, a, b, c, 8);
    R4(block, c, d, e, a, b, 9);
    R4(block, b, c, d, e, a, 10);
    R4(block, a, b, c, d, e, 11);
    R4(block, e, a, b, c, d, 12);
    R4(block, d, e, a, b, c, 13);
    R4(block, c, d, e, a, b, 14);
    R4(block, b, c, d, e, a, 15);

    digest[0] += a;
    digest[1] += b;
    digest[2] += c;
    digest[3] += d;
    digest[4] += e;
}

} // namespace sha1

struct SHA1Context {
    static unsigned int constexpr BlockSize = sha1::BLOCK_BYTES;
    static unsigned int constexpr DigestSize = 20;

    std::size_t _buflen;
    std::size_t _blocks;
    std::uint32_t _digest[5];
    std::uint8_t _buf[BlockSize];
    
    SHA1Context() 
        : _buflen{}
        , _blocks{}
        , _digest{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0}
    {}

    template <class = void>
    void update(void const* message, std::size_t size) noexcept {
        auto p = reinterpret_cast<std::uint8_t const*>(message);
        for (;;) {
            auto const n = (std::min)(size, sizeof(_buf) - _buflen);
            std::memcpy(_buf + _buflen, p, n);
            _buflen += n;
            if (_buflen != 64)
                return;
            p += n;
            size -= n;
            _buflen = 0;
            std::uint32_t block[sha1::BLOCK_INTS];
            sha1::make_block(_buf, block);
            sha1::transform(_digest, block);
            ++_blocks;
        }
    }
    
    template <class = void>
    void finish(void* digest) noexcept {
        using sha1::BLOCK_BYTES;
        using sha1::BLOCK_INTS;
    
        std::uint64_t total_bits = (_blocks * 64 + _buflen) * 8;
        // pad
        _buf[_buflen++] = 0x80;
        auto const buflen = _buflen;
        while (_buflen < 64)
            _buf[_buflen++] = 0x00;
        std::uint32_t block[BLOCK_INTS];
        sha1::make_block(_buf, block);
        if (buflen > BLOCK_BYTES - 8) {
            sha1::transform(_digest, block);
            for (size_t i = 0; i < BLOCK_INTS - 2; i++)
                block[i] = 0;
        }
    
        /* 附加total_bits, 将此uint64_t拆分为两个uint32_t */
        block[BLOCK_INTS - 1] = static_cast<uint32_t>(total_bits & 0xFFFF'FFFFU);
        block[BLOCK_INTS - 2] = static_cast<uint32_t>(total_bits >> 32U);
        sha1::transform(_digest, block);
        for (std::size_t i = 0; i < sha1::DIGEST_BYTES / 4; ++i) {
            std::uint8_t* d = reinterpret_cast<std::uint8_t*>(digest) + 4 * i;
            d[3] = static_cast<uint8_t>(_digest[i] >>  0) & 0xFF;
            d[2] = static_cast<uint8_t>(_digest[i] >>  8) & 0xFF;
            d[1] = static_cast<uint8_t>(_digest[i] >> 16) & 0xFF;
            d[0] = static_cast<uint8_t>(_digest[i] >> 24) & 0xFF;
        }
    }
};


} // namespace HX::net

#endif // !_HX_SHA1_H_