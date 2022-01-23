#ifndef __FAMGRAPH_INDEX_H__
#define __FAMGRAPH_INDEX_H__

#include <memory>
#include <cstdint>
#include <string>

namespace fgidx {
class dense_idx
{
  std::unique_ptr<uint64_t[]> idx;

  dense_idx(uint64_t[], uint32_t t_v_max);
  dense_idx(std::unique_ptr<uint64_t[]>, uint32_t t_v_max);

public:
  uint32_t const v_max;

  struct half_interval
  {
    uint64_t const begin;
    uint64_t const end_exclusive;
  };

  static dense_idx make_dense_idx(std::string const &filepath,
    uint64_t n_edges);

  half_interval operator[](uint32_t v) const noexcept;
};
}// namespace fgidx

#endif//__FAMGRAPH_INDEX_H__
