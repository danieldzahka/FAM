#ifndef FAM_CODEC_HPP
#define FAM_CODEC_HPP

#include <string>
#include <utility>
#include <memory>
#include <stdexcept>

namespace famgraph::tools {
struct Block
{
  constexpr static auto MAX_SIZE = (1U << 24) - 1;
  uint32_t num_vals;
  uint32_t delta_size;// in Bytes
  uint32_t AlignedWords(uint32_t align = 4U) const
  {
    auto const header = 2U;
    auto const bytes = delta_size * (num_vals - 1);
    return header + ((bytes + align - 1) / align);
  }

  uint32_t Pack() const
  {
    if (delta_size > 4 || num_vals > MAX_SIZE)
      throw std::runtime_error("Pack(): invalid param");
    uint32_t result = 0;
    result |= delta_size << 24;
    result |= num_vals;
    return result;
  }

  static Block Unpack(uint32_t packed) noexcept
  {
    auto const delta = packed >> 24;
    auto const num_vals = packed & ((1 << 24) - 1);
    return { num_vals, delta };
  }
};

struct CompressionOptions
{
  uint32_t min_block_size;
  uint32_t max_block_size;
  CompressionOptions(uint32_t min_block_size_, uint32_t max_block_size_);
};

std::pair<std::unique_ptr<uint32_t[]>, uint64_t>
  Compress(uint32_t *array, uint64_t n, CompressionOptions const& options);

template<typename T, typename Function>
void Apply(uint32_t *buffer, uint32_t n, Function const& f) noexcept
{
  auto acc = buffer[0];
  T *arr = reinterpret_cast<T *>(buffer + 1);
  f(acc);
  for (int i = 0; i < n - 1; ++i) {
    acc += arr[i];
    f(acc);
  }
}

template<typename Function>
void Decompress(uint32_t *buffer, uint64_t n, Function const& f) noexcept
{
  auto *end = buffer + n;
  auto *p = buffer;
  while (p < end) {
    auto const packed_block = p[0];
    auto const b = famgraph::tools::Block::Unpack(packed_block);
    switch (b.delta_size) {
    case 1:
      Apply<uint8_t>(p + 1, b.num_vals, f);
      break;
    case 2:
      Apply<uint16_t>(p + 1, b.num_vals, f);
      break;
    case 4:
      Apply<uint32_t>(p + 1, b.num_vals, f);
      break;
    }
    p += b.AlignedWords();
  }
}
}// namespace famgraph::tools

#endif// FAM_CODEC_HPP
