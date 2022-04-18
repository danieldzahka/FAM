#include <famgraph.hpp>
#include "FAM.hpp"
#include <fmt/core.h>//TODO: Delete this dep

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
famgraph::RemoteGraph::Buffer famgraph::RemoteGraph::GetChannelBuffer(
  int channel) const noexcept
{
  auto const &edge_window = this->edge_window_;
  auto *p = static_cast<char *>(edge_window.laddr);
  auto const length = (edge_window.length / this->fam_control_->rdma_channels_);

  return { p + length * channel, length };
}
uint32_t famgraph::RemoteGraph::max_v() const noexcept
{
  return this->idx_.v_max;
}
famgraph::RemoteGraph::Iterator famgraph::RemoteGraph::GetIterator(
  const famgraph::VertexRange &range,
  int channel) const noexcept
{
  return famgraph::RemoteGraph::Iterator(
    std::vector<VertexRange>{ range }, *this, channel);
}
famgraph::RemoteGraph::Iterator famgraph::RemoteGraph::GetIterator(
  const famgraph::VertexSubset &vertex_set,
  int channel) const noexcept
{
  return famgraph::RemoteGraph::Iterator(
    VertexSubset::ConvertToRanges(vertex_set), *this, channel);
}
famgraph::EdgeIndexType famgraph::RemoteGraph::Degree(
  VertexLabel v) const noexcept
{
  auto interval = this->idx_[v];
  return interval.end_exclusive - interval.begin;
}

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
  return { std::vector<VertexRange>{ range }, *this };
}
uint32_t famgraph::LocalGraph::max_v() const noexcept
{
  return this->idx_.v_max;
}
famgraph::LocalGraph::Iterator famgraph::LocalGraph::GetIterator(
  const famgraph::VertexSubset &vertex_set) const noexcept
{
  return famgraph::LocalGraph::Iterator(
    VertexSubset::ConvertToRanges(vertex_set), *this);
}
famgraph::EdgeIndexType famgraph::LocalGraph::Degree(
  famgraph::VertexLabel v) const noexcept
{
  auto interval = this->idx_[v];
  return interval.end_exclusive - interval.begin;
}

bool famgraph::RemoteGraph::Iterator::HasNext() noexcept
{
  while (this->current_range_ != this->ranges_.cend()) {
    if (this->current_vertex_ < this->current_range_->end_exclusive)
      return true;
    ++this->current_range_;
    if (this->current_range_ == this->ranges_.cend()) return false;
    this->current_vertex_ = this->current_range_->start;
  }
  return false;
}

// precondition: window should be filled
famgraph::AdjacencyList famgraph::RemoteGraph::Iterator::Next() noexcept
{
  auto const v = this->current_vertex_++;
  if (v >= this->current_window_[this->current_window_.size() - 1]
             .end_exclusive) {
    this->current_window_ = this->MaximalRange(v);
    this->FillWindow(this->current_window_);
    this->cursor = static_cast<uint32_t *>(this->edge_buffer_.p);
  }

  auto const [start_inclusive, end_exclusive] = this->graph_.idx_[v];
  auto const num_edges = end_exclusive - start_inclusive;

  auto edges = const_cast<const uint32_t *>(this->cursor);
  this->cursor += num_edges;

  return { v, num_edges, edges };
}
famgraph::RemoteGraph::Iterator::Iterator(std::vector<VertexRange> &&ranges,
  RemoteGraph const &graph,
  int channel)
  : ranges_(std::move(ranges)),
    current_range_(ranges_.begin()), current_vertex_{ ranges_.begin()
                                                          != ranges_.end()
                                                        ? current_range_->start
                                                        : 0 },
    graph_(graph), edge_buffer_{ graph.GetChannelBuffer(channel) }, channel_{
      channel
    }
{
  if (this->HasNext()) {
    this->current_window_ = this->MaximalRange(this->current_range_->start);
    this->FillWindow({ this->current_window_ });
    this->cursor = static_cast<uint32_t *>(this->edge_buffer_.p);
  }
}
std::vector<famgraph::VertexRange>
  famgraph::RemoteGraph::Iterator::MaximalRange(uint32_t range_start) noexcept
{
  std::vector<famgraph::VertexRange> vertex_runs;
  uint32_t range_end = range_start;
  auto const edge_capacity = this->edge_buffer_.length / sizeof(uint32_t);

  uint64_t edges_taken = 0;
  while (edges_taken <= edge_capacity
         && vertex_runs.size() < famgraph::max_outstanding_wr) {
    while (range_end < this->current_range_->end_exclusive) {

      auto const [start_inclusive, end_exclusive] =
        this->graph_.idx_[range_end];
      auto const num_edges = end_exclusive - start_inclusive;
      edges_taken += num_edges;

      if (edges_taken <= edge_capacity) {
        range_end++;
      } else {
        break;
      }
    }
    vertex_runs.push_back({ range_start, range_end });
    if (this->current_range_ < this->ranges_.cend()) {
      ++this->current_range_;
      this->current_vertex_ = this->current_range_->start;
    } else {
      break;
    }
  }

  return vertex_runs;
}
void famgraph::RemoteGraph::Iterator::FillWindow(
  std::vector<famgraph::VertexRange> range_list) noexcept
{
  if (range_list.size() == 0) return;
  if (range_list.front().start >= range_list.back().end_exclusive) return;
  auto *edges = static_cast<uint32_t volatile *>(this->edge_buffer_.p);
  auto end = 0;

  // Setup FamSegment Vector
  std::vector<FAM::FamSegment> fam_segments;
  for (auto &range : range_list) {
    auto const &start = this->graph_.idx_[range.start].begin;
    auto const &end_exclusive =
      this->graph_.idx_[range.end_exclusive - 1].end_exclusive;
    auto const length = end_exclusive - start;
    if (length == 0) continue;

    auto const raddr =
      this->graph_.adjacency_array_.raddr + start * sizeof(uint32_t);
    fam_segments.push_back({ raddr, (uint32_t)(length * sizeof(uint32_t)) });
    end = end + length;
  }

  if (end == 0) return;

  end -= 1;

  // 1) sign edge window
  edges[0] = famgraph::null_vert;
  edges[end] = famgraph::null_vert;
  // 2) post rdma

  auto const rkey = this->graph_.adjacency_array_.rkey;
  auto const lkey = this->graph_.edge_window_.lkey;

  this->graph_.fam_control_->Read(
    this->edge_buffer_.p, fam_segments, lkey, rkey, this->channel_);

  // 3) wait on data
  while (edges[0] == famgraph::null_vert || edges[end] == famgraph::null_vert) {
  }
}

// precondition: current_vertex should be in current range
bool famgraph::LocalGraph::Iterator::HasNext() noexcept
{
  while (this->current_range_ != this->ranges_.cend()) {
    if (this->current_vertex_ < this->current_range_->end_exclusive)
      return true;
    ++this->current_range_;
    if (this->current_range_ == this->ranges_.cend()) return false;
    this->current_vertex_ = this->current_range_->start;
  }
  return false;
}

// precondition: current_vertex_ is valid
famgraph::AdjacencyList famgraph::LocalGraph::Iterator::Next() noexcept
{
  auto const v = this->current_vertex_++;
  auto const [start_inclusive, end_exclusive] = this->graph_.idx_[v];
  auto const num_edges = end_exclusive - start_inclusive;
  auto const *edges = &this->graph_.adjacency_array_[start_inclusive];
  return { v, num_edges, edges };
}

famgraph::LocalGraph::Iterator::Iterator(std::vector<VertexRange> &&ranges,
  const famgraph::LocalGraph &graph)
  : ranges_(std::move(ranges)), current_range_(ranges_.begin()),
    current_vertex_(
      ranges_.begin() != ranges_.end() ? current_range_->start : 0),
    graph_(graph)
{}

famgraph::VertexSubset::VertexSubset(uint32_t max_v)
  : bitmap_(new std::uint64_t[Offset(max_v) + 1]), max_v_(max_v)
{
  this->Clear();
}
uint32_t famgraph::VertexSubset::GetMaxV() const noexcept { return max_v_; }

void famgraph::PrintVertexSubset(
  const famgraph::VertexSubset &vertex_subset) noexcept
{
  const auto max_v = vertex_subset.GetMaxV();
  fmt::print("(");
  for (VertexLabel i = 0; i <= max_v; ++i) {
    if (vertex_subset[i]) fmt::print("{}, ", i);
  }
  fmt::print(")\n");
}
