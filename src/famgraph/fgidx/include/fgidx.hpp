#ifndef __FAMGRAPH_INDEX_H__
#define __FAMGRAPH_INDEX_H__

#include <memory>
#include <cstdint>
#include <string>

namespace fgidx {
class DenseIndex
{
  std::unique_ptr<uint64_t const[]> idx;

  DenseIndex(uint64_t[], uint32_t t_v_max);
  DenseIndex(std::unique_ptr<uint64_t[]>, uint32_t t_v_max);

public:
  uint32_t const v_max;

  struct HalfInterval
  {
    uint64_t const begin;
    uint64_t const end_exclusive;
  };

  static DenseIndex CreateInstance(std::string const &filepath,
    uint64_t n_edges);

  HalfInterval operator[](uint32_t v) const noexcept;
};

struct AdjacencyArray
{
  const uint64_t edges;
  std::unique_ptr<uint32_t[]> array;
};
AdjacencyArray CreateAdjacencyArray(std::string const &filepath);

}// namespace fgidx

#endif//__FAMGRAPH_INDEX_H__
