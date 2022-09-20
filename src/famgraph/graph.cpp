#include <famgraph.hpp>
#include "FAM.hpp"
#include <fmt/core.h>//TODO: Delete this dep

famgraph::RemoteGraph famgraph::RemoteGraph::CreateInstance(
  std::string const& index_file,
  std::string const& adj_file,
  std::string const& grpc_addr,
  std::string const& ipoib_addr,
  std::string const& ipoib_port,
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
famgraph::RemoteGraph::RemoteGraph(fgidx::DenseIndex&& idx,
  std::unique_ptr<FAM::FamControl>&& fam_control,
  FAM::FamControl::RemoteRegion adjacency_array,
  FAM::FamControl::LocalRegion edge_window)
  : idx_{ std::move(idx) }, fam_control_{ std::move(fam_control) },
    adjacency_array_{ adjacency_array }, edge_window_{ edge_window }
{}
famgraph::RemoteGraph::Buffer famgraph::RemoteGraph::GetChannelBuffer(
  int channel) const noexcept
{
  auto const& edge_window = this->edge_window_;
  auto *p = static_cast<char *>(edge_window.laddr);
  auto const length = (edge_window.length / this->fam_control_->rdma_channels_);

  return { p + length * channel, length };
}
uint32_t famgraph::RemoteGraph::max_v() const noexcept
{
  return this->idx_.v_max;
}
famgraph::EdgeIndexType famgraph::RemoteGraph::Degree(
  VertexLabel v) const noexcept
{
  auto interval = this->idx_[v];
  return interval.end_exclusive - interval.begin;
}
void famgraph::RemoteGraph::PostSegmentsAndWait(
  std::vector<FAM::FamSegment> const& segments,
  std::uint32_t taken,
  int channel) noexcept
{
  auto const [buffer, unused] = this->GetChannelBuffer(channel);
  auto *edges = static_cast<uint32_t volatile *>(buffer);
  auto const end = taken - 1;
  edges[0] = famgraph::null_vert;
  edges[end] = famgraph::null_vert;
  auto const rkey = this->adjacency_array_.rkey;
  auto const lkey = this->edge_window_.lkey;
  this->fam_control_->Read(buffer, segments, lkey, rkey, channel);
  while (edges[0] == famgraph::null_vert || edges[end] == famgraph::null_vert) {
  }
}

famgraph::LocalGraph::LocalGraph(fgidx::DenseIndex&& idx,
  std::unique_ptr<uint32_t[]>&& adjacency_array)
  : idx_(std::move(idx)), adjacency_array_(std::move(adjacency_array))
{}

famgraph::LocalGraph famgraph::LocalGraph::CreateInstance(
  std::string const& index_file,
  std::string const& adj_file)
{
  auto [edges, array] = fgidx::CreateAdjacencyArray(adj_file);
  return { fgidx::DenseIndex::CreateInstance(index_file, edges),
    std::move(array) };
}

uint32_t famgraph::LocalGraph::max_v() const noexcept
{
  return this->idx_.v_max;
}
famgraph::EdgeIndexType famgraph::LocalGraph::Degree(
  famgraph::VertexLabel v) const noexcept
{
  auto interval = this->idx_[v];
  return interval.end_exclusive - interval.begin;
}

famgraph::VertexSubset::VertexSubset(uint32_t max_v)
  : bitmap_(new std::uint64_t[Offset(max_v) + 1]), max_v_(max_v)
{
  this->Clear();
}
uint32_t famgraph::VertexSubset::GetMaxV() const noexcept { return max_v_; }

void famgraph::PrintVertexSubset(
  famgraph::VertexSubset const& vertex_subset) noexcept
{
  auto const max_v = vertex_subset.GetMaxV();
  fmt::print("(");
  for (VertexLabel i = 0; i <= max_v; ++i) {
    if (vertex_subset[i]) fmt::print("{}, ", i);
  }
  fmt::print(")\n");
}
