#include <famgraph.hpp>
#include "FAM.hpp"

famgraph::RemoteGraph famgraph::RemoteGraph::CreateInstance(
  std::string const &index_file,
  std::string const &adj_file,
  std::string const &grpc_addr,
  std::string const &ipoib_addr,
  std::string const &ipoib_port,
  int const rdma_channels)
{
  auto fam_control = std::make_unique<FAM::FamControl>(
    grpc_addr, ipoib_addr, ipoib_port, rdma_channels);
  auto const adjacency_file = fam_control->MmapRemoteFile(adj_file);
  uint64_t const edges = adjacency_file.length / sizeof(uint32_t);

  return RemoteGraph{ fgidx::DenseIndex::CreateInstance(index_file, edges),
    std::move(fam_control) };
}
famgraph::RemoteGraph::RemoteGraph(fgidx::DenseIndex &&t_idx,
  std::unique_ptr<FAM::FamControl> &&t_fam_control)
  : idx_{ std::move(t_idx) }, fam_control_{ std::move(t_fam_control) }
{}