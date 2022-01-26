#ifndef __FAMGRAPH_INDEX_H__
#define __FAMGRAPH_INDEX_H__

#include <memory>
#include <cstdint>
#include <string>

namespace fgidx {
class DenseIndex
{
  std::unique_ptr<uint64_t const[]> idx;

  DenseIndex(uint64_t t_idx[],
    uint32_t const t_v_max,
    uint64_t const t_max_out_degree);
  DenseIndex(std::unique_ptr<uint64_t[]> t_idx,
    uint32_t t_v_max,
    uint64_t t_max_out_degree);

public:
  uint32_t const v_max;
  uint64_t const max_out_degree;

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
