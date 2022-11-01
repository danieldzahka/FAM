#ifndef FAM_NOP_DECOMPRESSOR_HPP
#define FAM_NOP_DECOMPRESSOR_HPP

#include <cstdint>

struct NopDecompressor
{
  template<typename Function>
  static void
    Decompress(uint32_t const *buffer, uint64_t n, Function const& f) noexcept
  {
    for (uint32_t i = 0; i < n; ++i) { f(buffer[i], n); }
  }
};

#endif// FAM_NOP_DECOMPRESSOR_HPP
