#ifndef __FAMGRAPH_H__
#define __FAMGRAPH_H__

#include <memory>
#include <cstdint>
#include <fgidx.hpp>
#include <FAM.hpp>

namespace famgraph {
class RemoteGraph
{
  std::unique_ptr<FAM::FamControl> fam_control_;
  fgidx::DenseIndex idx_;
  RemoteGraph(fgidx::DenseIndex &&t_idx,
    std::unique_ptr<FAM::FamControl> &&t_fam_control);

public:
  static famgraph::RemoteGraph CreateInstance(std::string const &index_file,
    std::string const &adj_file,
    std::string const &grpc_addr,
    std::string const &ipoib_addr,
    std::string const &ipoib_port,
    int const rdma_channels);
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
