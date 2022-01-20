#ifndef __FAMGRAPH_H__
#define __FAMGRAPH_H__

#include <cstdint>

namespace famgraph {
// bfs
// pagerank
// CC
// kcore
// MIS

struct kcore_result
{
  uint32_t const k;
  uint32_t const core_size;
};

kcore_result kcore();
}// namespace famgraph

#endif//__FAMGRAPH_H__
