#include <catch2/catch.hpp>

#include <codec.hpp>
#include <vector>
#include <random>
#include <iostream>

TEST_CASE("Pack and Unpack")
{
  std::vector<uint32_t> deltas{ 1, 2, 4 };
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::mt19937::result_type> dist(
    1, famgraph::tools::Block::MAX_SIZE);
  std::uniform_int_distribution<std::mt19937::result_type> delta_dist(
    0, deltas.size() - 1);
  auto const test_cases = 10000;
  for (int i = 0; i < test_cases; ++i) {
    auto const delta_size = deltas[delta_dist(rd)];
    auto const num_vals = static_cast<uint32_t>(dist(rd));
    auto const block = famgraph::tools::Block{ num_vals, delta_size };
    auto const packed = block.Pack();
    auto const [v, d] = famgraph::tools::Block::Unpack(packed);
    REQUIRE(v == num_vals);
    REQUIRE(d == delta_size);
  }
}

TEST_CASE("Compress Decompress")
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::mt19937::result_type> dist(
    0, std::numeric_limits<uint32_t>::max() - 1);
  std::uniform_int_distribution<std::mt19937::result_type> size(1, 500000);
  auto const test_cases = 10;
  for (int i = 0; i < test_cases; ++i) {
    auto const s = size(gen);
    std::vector<uint32_t> v;
    for (int j = 0; j < s; ++j) {
      auto const x = dist(gen);
      v.push_back(x);
    }
    sort(v.begin(), v.end());
    auto const p = v.data();
    auto const n = v.size();
    famgraph::tools::CompressionOptions options{ 10, 1000 };
    auto const [compressed, count] = famgraph::tools::Compress(p, n, options);
    std::vector<uint32_t> other;
    auto const build = [&](uint32_t x) { other.push_back(x); };
    famgraph::tools::Decompress(compressed.get(), count, build);
    REQUIRE(v == other);
  }
}