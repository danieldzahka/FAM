#ifndef __FAMGRAPH_H__
#define __FAMGRAPH_H__

#include <cstdint>
#include <fgidx.hpp>

namespace famgraph {
class RemoteGraph
{
  RemoteGraph(fgidx::DenseIndex &&t_idx);
  fgidx::DenseIndex idx_;

public:
  static RemoteGraph CreateInstance(std::string const &path);
  class Iterator
  {
  };
};
class LocalGraph
{
  fgidx::DenseIndex idx_;

public:
  class Iterator
  {
  };
};

}// namespace famgraph

#endif//__FAMGRAPH_H__
