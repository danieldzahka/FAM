#ifndef __FAMGRAPH_INDEX_H__
#define __FAMGRAPH_INDEX_H__

#include <memory>
#include <cstdint>
#include <string>

namespace fgidx {
class dense_idx
{
  std::unique_ptr<uint64_t[]> idx;

  explicit dense_idx(uint64_t[]);
  explicit dense_idx(std::unique_ptr<uint64_t[]>);

public:
  struct half_interval
  {
    uint64_t const begin;
    uint64_t const end_exclusive;
  };

  static dense_idx make_dense_idx(std::string const &filepath,
    uint64_t const n_edges);

  half_interval operator[](const uint32_t v) const noexcept;
};
}// namespace fgidx

#endif//__FAMGRAPH_INDEX_H__
