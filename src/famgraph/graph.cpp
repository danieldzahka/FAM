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

  auto index = fgidx::DenseIndex::CreateInstance(index_file, edges);

  auto const edge_window_size = index.max_out_degree
                                * static_cast<unsigned long>(rdma_channels)
                                * sizeof(uint32_t);
  auto const edge_window =
    fam_control->CreateRegion(edge_window_size, false, true);

  return RemoteGraph{
    std::move(index), std::move(fam_control), adjacency_file, edge_window
  };
}
famgraph::RemoteGraph::RemoteGraph(fgidx::DenseIndex &&idx,
  std::unique_ptr<FAM::FamControl> &&fam_control,
  FAM::FamControl::RemoteRegion adjacency_array,
  FAM::FamControl::LocalRegion edge_window)
  : idx_{ std::move(idx) }, fam_control_{ std::move(fam_control) },
    adjacency_array_{ adjacency_array }, edge_window_{ edge_window }
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
  return { fgidx::DenseIndex::CreateInstance(index_file, edges),
    std::move(array) };
}

famgraph::LocalGraph::Iterator famgraph::LocalGraph::GetIterator(
  const famgraph::VertexRange &range) const noexcept
{
  return { range, *this };
}
uint32_t famgraph::LocalGraph::max_v() const noexcept
{
  return this->idx_.v_max;
}

bool famgraph::RemoteGraph::Iterator::HasNext() const noexcept
{
  return this->current_vertex_ < this->range_.end_exclusive;
}
famgraph::AdjacencyList famgraph::RemoteGraph::Iterator::Next() noexcept
{
  auto const v = this->current_vertex_++;
  //  auto const [start_inclusive, end_exclusive] = this->graph_.idx_[v];
  //  auto const num_edges = end_exclusive - start_inclusive;
  //  auto const *edges = &this->graph_.adjacency_array_[start_inclusive];
  //
  // auto const [start, end] =
  return { 0U, 0U, nullptr };
}
famgraph::RemoteGraph::Iterator::Iterator(const VertexRange &range,
  RemoteGraph const &graph,
  int channel)
  : range_(range), current_vertex_{ range.start }, graph_(graph)
{}
famgraph::VertexRange famgraph::RemoteGraph::Iterator::MaximalRange(
  uint32_t range_start)
{
  uint32_t range_end = range_start;
  auto const edge_capacity =
    this->graph_.edge_window_.length / sizeof(uint32_t);
  uint64_t edges_taken = 0;
  while (
    edges_taken < edge_capacity && range_end < this->range_.end_exclusive) {}

  return { range_start, range_end };
}
bool famgraph::LocalGraph::Iterator::HasNext() const noexcept
{
  return this->current_vertex_ < this->range_.end_exclusive;
}

famgraph::AdjacencyList famgraph::LocalGraph::Iterator::Next() noexcept
{
  auto const v = this->current_vertex_++;
  auto const [start_inclusive, end_exclusive] = this->graph_.idx_[v];
  auto const num_edges = end_exclusive - start_inclusive;
  auto const *edges = &this->graph_.adjacency_array_[start_inclusive];
  return { v, num_edges, edges };
}

famgraph::LocalGraph::Iterator::Iterator(const famgraph::VertexRange &range,
  const famgraph::LocalGraph &graph)
  : range_(range), current_vertex_(range_.start), graph_(graph)
{}
