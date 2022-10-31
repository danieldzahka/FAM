#include <codec.hpp>
#include <cstdint>
#include <stdexcept>

#include <fmt/core.h>
#include <vector>
#include <algorithm>

namespace {

uint32_t DeltaSize(uint32_t diff)
{
  if (diff <= std::numeric_limits<uint8_t>::max()) return 1;
  if (diff <= std::numeric_limits<uint16_t>::max()) return 2;
  return 4;
}

famgraph::tools::Block DetermineNextBlock(uint32_t *vals,
  uint64_t n,
  famgraph::tools::CompressionOptions const& options)
{
  auto const min_block_size = options.min_block_size;
  auto const max_block_size = options.max_block_size;
  if (n == 1) return famgraph::tools::Block{ 1, 4 };

  auto delta = 1U;
  auto taken = 1U;
  auto const max = std::min(static_cast<uint32_t>(n), max_block_size);
  for (int i = 1; i < max; ++i) {
    if (vals[i - 1] > vals[i])
      throw std::runtime_error("Cannot compress sequence with inversion");
    auto const d = DeltaSize(vals[i] - vals[i - 1]);
    if (d > delta) {
      if (i < min_block_size) {
        delta = d;
      } else {
        break;
      }
    }
    ++taken;
  }

  return famgraph::tools::Block{ taken, delta };
}

template<typename T> void Write(uint32_t *out, uint32_t *in, uint32_t n)
{
  out[0] = in[0];
  T *p = reinterpret_cast<T *>(out + 1);
  for (int i = 1; i < n; ++i) {
    T const diff = in[i] - in[i - 1];
    p[i - 1] = diff;
  }
}

}// namespace

std::pair<std::unique_ptr<uint32_t[]>, uint64_t> famgraph::tools::Compress(
  uint32_t *array,
  uint64_t n,
  CompressionOptions const& options)
{
  if (n == 0) return std::pair(nullptr, 0);

  auto remaining = n;
  auto *p = array;
  std::vector<Block> blocks;
  auto output_4B_words = 0U;
  while (remaining > 0) {
    auto const b = DetermineNextBlock(p, remaining, options);
    remaining -= b.num_vals;
    p += b.num_vals;
    blocks.push_back(b);
    output_4B_words += b.AlignedWords();
  }

  auto output = new uint32_t[output_4B_words];
  uint32_t *out = output;
  auto *in = array;
  for (auto const block : blocks) {
    out[0] = block.Pack();
    if (block.delta_size == 1) {
      Write<uint8_t>(out + 1, in, block.num_vals);
    } else if (block.delta_size == 2) {
      Write<uint16_t>(out + 1, in, block.num_vals);
    } else {
      Write<uint32_t>(out + 1, in, block.num_vals);
    }

    in += block.num_vals;
    out += block.AlignedWords();
  }

  return std::pair(std::unique_ptr<uint32_t[]>(output), output_4B_words);
}
famgraph::tools::CompressionOptions::CompressionOptions(
  uint32_t min_block_size_,
  uint32_t max_block_size_)
  : min_block_size(min_block_size_), max_block_size(max_block_size_)
{
  if (min_block_size_ > max_block_size) {
    throw std::runtime_error(
      fmt::format("{} is not a valid block size", min_block_size_));
  }

  if (max_block_size > famgraph::tools::Block::MAX_SIZE)
    throw std::runtime_error("max block size is too big");
}
