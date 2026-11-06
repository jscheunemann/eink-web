#pragma once
// Browser build: self-contained MD5 (RFC 1321) for the simulator's MD5Builder
// API. The desktop shim uses OpenSSL or CommonCrypto, neither of which exists
// under Emscripten.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "WString.h"

class MD5Builder {
 public:
  MD5Builder() { memset(digest_, 0, sizeof(digest_)); }

  void begin() {
    a0_ = 0x67452301;
    b0_ = 0xefcdab89;
    c0_ = 0x98badcfe;
    d0_ = 0x10325476;
    len_ = 0;
    bufLen_ = 0;
  }

  void add(const uint8_t *data, size_t len) {
    len_ += len;
    while (len > 0) {
      const size_t take = (64 - bufLen_) < len ? (64 - bufLen_) : len;
      memcpy(buf_ + bufLen_, data, take);
      bufLen_ += take;
      data += take;
      len -= take;
      if (bufLen_ == 64) {
        block(buf_);
        bufLen_ = 0;
      }
    }
  }

  void add(const char *str) {
    if (str) add(reinterpret_cast<const uint8_t *>(str), strlen(str));
  }

  void calculate() {
    const uint64_t bits = len_ * 8;
    const uint8_t pad = 0x80;
    add(&pad, 1);
    const uint8_t zero = 0;
    while (bufLen_ != 56) add(&zero, 1);
    uint8_t lenBytes[8];
    for (int i = 0; i < 8; i++) lenBytes[i] = static_cast<uint8_t>(bits >> (8 * i));
    add(lenBytes, 8);
    const uint32_t words[4] = {a0_, b0_, c0_, d0_};
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++) digest_[i * 4 + j] = static_cast<uint8_t>(words[i] >> (8 * j));
  }

  String toString() const {
    char hex[33];
    for (int i = 0; i < 16; i++) snprintf(hex + i * 2, 3, "%02x", digest_[i]);
    return String(hex);
  }

 private:
  static uint32_t rotl(uint32_t x, uint32_t c) { return (x << c) | (x >> (32 - c)); }

  void block(const uint8_t *p) {
    static const uint32_t s[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                   5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                   4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                   6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    static const uint32_t k[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    uint32_t m[16];
    for (int i = 0; i < 16; i++)
      m[i] = static_cast<uint32_t>(p[i * 4]) | (static_cast<uint32_t>(p[i * 4 + 1]) << 8) |
             (static_cast<uint32_t>(p[i * 4 + 2]) << 16) | (static_cast<uint32_t>(p[i * 4 + 3]) << 24);
    uint32_t a = a0_, b = b0_, c = c0_, d = d0_;
    for (uint32_t i = 0; i < 64; i++) {
      uint32_t f, g;
      if (i < 16) {
        f = (b & c) | (~b & d);
        g = i;
      } else if (i < 32) {
        f = (d & b) | (~d & c);
        g = (5 * i + 1) % 16;
      } else if (i < 48) {
        f = b ^ c ^ d;
        g = (3 * i + 5) % 16;
      } else {
        f = c ^ (b | ~d);
        g = (7 * i) % 16;
      }
      f = f + a + k[i] + m[g];
      a = d;
      d = c;
      c = b;
      b = b + rotl(f, s[i]);
    }
    a0_ += a;
    b0_ += b;
    c0_ += c;
    d0_ += d;
  }

  uint32_t a0_ = 0x67452301, b0_ = 0xefcdab89, c0_ = 0x98badcfe, d0_ = 0x10325476;
  uint64_t len_ = 0;
  uint8_t buf_[64];
  size_t bufLen_ = 0;
  uint8_t digest_[16];
};
