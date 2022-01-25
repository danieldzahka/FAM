#include <famgraph.hpp>
#include "FAM.hpp"

famgraph::RemoteGraph famgraph::RemoteGraph::CreateInstance(
  std::string const &index_file,
  std::string const &adj_file,
  std::string const &grpc_addr,
  std::string const &ipoib_addr,
  std::string const &ipoib_port,
  int rdma_channels)
{
  auto fam_control = std::make_unique<FAM::FamControl>(
    grpc_addr, ipoib_addr, ipoib_port, rdma_channels);
  auto const adjacency_file = fam_control->MmapRemoteFile(adj_file);
  uint64_t const edges = adjacency_file.length / sizeof(uint32_t);

  return RemoteGraph{ fgidx::DenseIndex::CreateInstance(index_file, edges),
    std::move(fam_control),
    adjacency_file };
}
famgraph::RemoteGraph::RemoteGraph(fgidx::DenseIndex &&idx,
  std::unique_ptr<FAM::FamControl> &&fam_control,
  FAM::FamControl::RemoteRegion adjacency_array)
  : idx_{ std::move(idx) }, fam_control_{ std::move(fam_control) },
    adjacency_array_{ adjacency_array }
{}

famgraph::LocalGraph::LocalGraph(fgidx::DenseIndex &&idx,
  std::unique_ptr<uint32_t[]> &&adjacency_array)
  : idx_(std::move(idx)), adjacency_array_(std::move(adjacency_array))
{}
famgraph::LocalGraph famgraph::LocalGraph::CreateInstance(
  const std::string &index_file,
  const std::string &adj_file)
{
  auto [edges, array] = fgidx::CreateAdjacencyArray(adj_file);
  return famgraph::LocalGraph(
    fgidx::DenseIndex::CreateInstance(index_file, edges), std::move(array));
}
