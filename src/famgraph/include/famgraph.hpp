#ifndef __FAMGRAPH_H__
#define __FAMGRAPH_H__

#include <range/v3/all.hpp>
#include <memory>
#include <cstring>
#include <cstdint>
#include <fgidx.hpp>
#include <FAM.hpp>
#include <FAM_constants.hpp>
#include <nop_decompressor.hpp>
#include <codec.hpp>

#include <oneapi/tbb.h>

namespace famgraph {
using VertexLabel = std::uint32_t;
using EdgeIndexType = std::uint64_t;
constexpr uint32_t null_vert = std::numeric_limits<std::uint32_t>::max();

enum class TbbDispatch { USE_TBB };

struct VertexRange
{
  uint32_t start;
  uint32_t end_exclusive;
};

struct AdjacencyList
{
  uint32_t const v;
  uint64_t const num_edges;
  uint32_t const *edges;
};

class VertexSubset
{
  std::unique_ptr<std::uint64_t[]> bitmap_;
  std::uint32_t const max_v_;
  tbb::combinable<VertexLabel> num_active_;

  constexpr static std::uint32_t Offset(std::uint32_t v) { return v >> 6; }
  constexpr static std::uint32_t BitOffset(std::uint32_t v)
  {
    return v & ((1 << 6) - 1);
  }

public:
  explicit VertexSubset(uint32_t max_v);

  bool operator[](std::uint32_t v) const noexcept
  {
    auto const word = Offset(v);
    auto const bit_offset = BitOffset(v);
    return this->bitmap_[word] & (1UL << bit_offset);
  }

  bool Set(std::uint32_t v) noexcept
  {
    auto& word = this->bitmap_[Offset(v)];
    auto const bit_offset = BitOffset(v);
    auto prev = __sync_fetch_and_or(&word, 1UL << bit_offset);
    bool const was_unset = !(prev & (1UL << bit_offset));
    if (was_unset) ++this->num_active_.local();
    return was_unset;
  }

  [[nodiscard]] bool IsEmpty() noexcept
  {
    return this->num_active_.combine(std::plus<uint32_t>{}) == 0;
  }

  [[nodiscard]] bool IsEmpty2() noexcept
  {
    uint64_t acc = 0;
    for (uint64_t k = 0; k < this->Count(); ++k) { acc |= this->bitmap_[k]; }
    return !acc;
  }


  // TODO: Make parallel clear
  void Clear() noexcept
  {
    this->num_active_.clear();
    std::memset(this->bitmap_.get(),
      0,
      sizeof(std::uint64_t) * (1 + Offset(this->max_v_)));
  }

  // TODO: Fix bug where extra 1's are at end. Note that the range conversion
  void SetAll() noexcept
  {
    this->num_active_.local() = this->max_v_ + 1;
    std::memset(this->bitmap_.get(),
      0xFF,
      sizeof(std::uint64_t) * (1 + Offset(this->max_v_)));
    //    auto &word = this->bitmap_[Offset(this->max_v_)];
    //    auto const bit_offset = BitOffset(this->max_v_);
    //    auto const mask = (1 << (bit_offset + 1)) - 1;
    //    word &= mask;
  }

  [[nodiscard]] uint32_t GetMaxV() const noexcept;
  std::uint64_t *GetTable() noexcept;
  std::uint32_t Count() noexcept;
};

void PrintVertexSubset(VertexSubset const& vertex_subset) noexcept;

template<typename Decompressor = NopDecompressor> class RemoteGraph
{
  fgidx::DenseIndex const idx_;
  std::unique_ptr<FAM::FamControl> fam_control_;
  FAM::FamControl::RemoteRegion const adjacency_array_;
  FAM::FamControl::LocalRegion edge_window_;

  RemoteGraph(fgidx::DenseIndex&& idx,
    std::unique_ptr<FAM::FamControl>&& fam_control,
    FAM::FamControl::RemoteRegion adjacency_array,
    FAM::FamControl::LocalRegion edge_window)
    : idx_{ std::move(idx) }, fam_control_{ std::move(fam_control) },
      adjacency_array_{ adjacency_array }, edge_window_{ edge_window }
  {}

  void PostSegmentsAndWait(std::vector<FAM::FamSegment> const& segments,
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
    this->fam_control_->Read(
      buffer, segments, lkey, rkey, static_cast<unsigned long>(channel));
    while (
      edges[0] == famgraph::null_vert || edges[end] == famgraph::null_vert) {}
  }


  struct SegmentDescriptor
  {
    VertexLabel v;
  };

  template<typename Range>
  std::tuple<std::vector<SegmentDescriptor>,
    std::vector<FAM::FamSegment>,
    std::uint32_t>
    GetSegments(Range r) noexcept
  {
    [[maybe_unused]] auto const [unused, length] = this->GetChannelBuffer(0);
    auto const capacity = length / sizeof(VertexLabel);
    std::vector<SegmentDescriptor> descriptors;
    std::vector<FAM::FamSegment> segments;
    std::uint32_t taken = 0;
    VertexLabel last_taken = 0;
    for (auto const v : r) {
      auto const is_continue = (v == last_taken + 1) && segments.size() != 0;
      auto const [start_inclusive, end_exclusive] = this->idx_[v];
      auto const edges = static_cast<uint32_t>(end_exclusive - start_inclusive);
      if (edges == 0) continue;
      if (taken + edges > capacity) break;

      auto const length2 = edges * static_cast<uint32_t>(sizeof(VertexLabel));

      if (is_continue) {
        segments.back().length += length2;
        taken += edges;
        last_taken = v;
        descriptors.push_back({ v });
        continue;
      }

      if (segments.size() >= FAM::max_outstanding_wr) break;

      auto const raddr =
        this->adjacency_array_.raddr + start_inclusive * sizeof(VertexLabel);
      segments.push_back({ raddr, length2 });
      taken += edges;
      last_taken = v;
      descriptors.push_back({ v });
    }

    return std::tuple(descriptors, segments, taken);
  }

public:
  static auto CreateInstance(std::string const& index_file,
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

  uint32_t max_v() const noexcept { return this->idx_.v_max; }

  famgraph::EdgeIndexType Degree(VertexLabel v) const noexcept
  {
    auto interval = this->idx_[v];
    return interval.end_exclusive - interval.begin;
  }

  struct Buffer
  {
    void *const p;
    uint64_t const length;
  };

  Buffer GetChannelBuffer(int channel) const noexcept
  {
    auto const& edge_window = this->edge_window_;
    auto *p = static_cast<char *>(edge_window.laddr);
    auto const length =
      (edge_window.length
        / static_cast<unsigned long>(this->fam_control_->rdma_channels_));

    return { p + length * static_cast<unsigned long>(channel), length };
  }

  template<typename Function, typename Filter>
  void EdgeMap(Function f,
    Filter const& is_active,
    ranges::iota_view<std::uint32_t, std::uint32_t> range,
    int channel = 0)
  {
    if (range.empty()) return;
    auto next_start = range.front();
    auto const last = range.back();
    while (next_start <= last) {
      // 1) build up vector of intervals
      auto r = ranges::views::iota(next_start, last + 1)
               | ranges::views::filter(is_active);

      auto const [descriptors, segments, taken] = this->GetSegments(r);
      if (segments.empty()) return;
      if (taken == 0) return;// need return here?... no we don't

      next_start = descriptors.back().v + 1;

      // 2) post the RDMA request
      this->PostSegmentsAndWait(segments, taken, channel);
      // 3) traverse the vector
      [[maybe_unused]] auto const [buffer, length] =
        this->GetChannelBuffer(channel);
      auto b = static_cast<uint32_t *>(buffer);
      for (auto const [v] : descriptors) {
        auto const [start_inclusive, end_exclusive] = this->idx_[v];
        auto const num_edges = end_exclusive - start_inclusive;
        Decompressor::Decompress(b,
          num_edges,
          [&](uint32_t dst, uint32_t degree) { f(v, dst, degree); });
        //        for (unsigned int i = 0; i < num_edges; ++i) f(v, b[i],
        //        num_edges);
        b += num_edges;
      }
    }
  }

  template<typename Function>
  void EdgeMap(Function& F, VertexSubset const& subset)
  {
    auto is_active = [&](auto v) { return subset[v]; };
    this->EdgeMap(F, is_active, { 0, subset.GetMaxV() + 1 });
  }

  template<typename Function>
  void EdgeMap(Function& F,
    VertexSubset const& subset,
    ranges::iota_view<std::uint32_t, std::uint32_t> range,
    int channel = 0)
  {
    auto is_active = [&](auto v) { return subset[v]; };
    this->EdgeMap(F, is_active, range, channel);
  }

  template<typename Function> void EdgeMap(Function& F)
  {
    auto is_active = [](auto) { return true; };
    this->EdgeMap(F, is_active, { 0, this->max_v() + 1 });
  }
};

template<>
inline famgraph::EdgeIndexType RemoteGraph<tools::DeltaDecompressor>::Degree(
  famgraph::VertexLabel v) const noexcept
{
  auto interval = this->idx_[v];
  if (interval.end_exclusive - interval.begin == 0) return 0;
  auto const channel = tbb::this_task_arena::current_thread_index();
  [[maybe_unused]] auto const [buffer, unused] =
    this->GetChannelBuffer(channel);
  uint32_t constexpr magic = 0xFFFFFFFF;
  uint32_t volatile *p = reinterpret_cast<uint32_t volatile *>(buffer);
  *p = magic;
  auto const l = sizeof(uint32_t);
  auto const rkey = this->adjacency_array_.rkey;
  auto const lkey = this->edge_window_.lkey;
  auto const raddr =
    this->adjacency_array_.raddr + interval.begin * sizeof(VertexLabel);
  this->fam_control_->Read(buffer, raddr, l, lkey, rkey, channel);
  while (*p == magic) {}
  return *p;
}

template<typename Decompressor = NopDecompressor> class LocalGraph
{
  fgidx::DenseIndex idx_;
  std::unique_ptr<uint32_t[]> adjacency_array_;

  LocalGraph(fgidx::DenseIndex&& idx,
    std::unique_ptr<uint32_t[]>&& adjacency_array)
    : idx_(std::move(idx)), adjacency_array_(std::move(adjacency_array))
  {}

public:
  static LocalGraph CreateInstance(std::string const& index_file,
    std::string const& adj_file)
  {
    auto [edges, array] = fgidx::CreateAdjacencyArray(adj_file);
    return { fgidx::DenseIndex::CreateInstance(index_file, edges),
      std::move(array) };
  }


  uint32_t max_v() const noexcept { return this->idx_.v_max; }

  famgraph::EdgeIndexType Degree(famgraph::VertexLabel v) const noexcept
  {
    auto interval = this->idx_[v];
    return interval.end_exclusive - interval.begin;
  }

  template<typename Function, typename Filter>
  void EdgeMap(Function& f,
    Filter const& is_active,
    ranges::iota_view<std::uint32_t, std::uint32_t> range)
  {
    for (auto const v : range | ranges::views::filter(is_active)) {
      auto const [start_inclusive, end_exclusive] = this->idx_[v];
      auto const num_edges = end_exclusive - start_inclusive;
      auto const *edges = &this->adjacency_array_[start_inclusive];
      Decompressor::Decompress(edges,
        num_edges,
        [&](uint32_t dst, uint32_t degree) { f(v, dst, degree); });
      //      for (unsigned int i = 0; i < num_edges; ++i) {
      //        f(v, edges[i], num_edges);
      //      }
    }
  }
  template<typename Function>
  void EdgeMap(Function& F, VertexSubset const& subset)
  {
    auto is_active = [&](auto v) { return subset[v]; };
    this->EdgeMap(F, is_active, { 0, subset.GetMaxV() + 1 });
  }

  template<typename Function>
  void EdgeMap(Function& F,
    VertexSubset const& subset,
    ranges::iota_view<std::uint32_t, std::uint32_t> range,
    int /*channel*/ = 0)
  {
    auto is_active = [&](auto v) { return subset[v]; };
    this->EdgeMap(F, is_active, range);
  }

  template<typename Function> void EdgeMap(Function& F)
  {
    auto is_active = [](auto) { return true; };
    this->EdgeMap(F, is_active, { 0, this->max_v() + 1 });
  }
};

template<>
inline famgraph::EdgeIndexType LocalGraph<tools::DeltaDecompressor>::Degree(
  famgraph::VertexLabel v) const noexcept
{
  auto interval = this->idx_[v];
  if (interval.end_exclusive - interval.begin == 0) return 0;
  return this->adjacency_array_[interval.begin];
}

template<typename Vertex, typename AdjancencyGraph> class Graph
{
  AdjancencyGraph& adjacency_graph_;
  std::unique_ptr<Vertex[]> vertex_array_;

public:
  explicit Graph(AdjancencyGraph& adjacency_graph)
    : adjacency_graph_(adjacency_graph),
      vertex_array_(new Vertex[adjacency_graph_.max_v() + 1])
  {}

  auto max_v() const noexcept { return this->adjacency_graph_.max_v(); }
  [[nodiscard]] auto Degree(VertexLabel v) const noexcept
  {
    return this->adjacency_graph_.Degree(v);
  };

  Vertex& operator[](std::uint32_t v) noexcept
  {
    return this->vertex_array_[v];
  }

  AdjancencyGraph& getAdjacencyGraph() const noexcept
  {
    return adjacency_graph_;
  }

  Vertex *VertexArray() const noexcept { return this->vertex_array_.get(); }

  [[nodiscard]] VertexLabel NumVertices() const noexcept
  {
    return this->adjacency_graph_.max_v() + 1;
  }
};

template<typename Graph, typename VertexProgram>
void EdgeMap(Graph& graph,
  VertexSubset const& subset,
  VertexProgram& f,
  tbb::blocked_range<VertexLabel> const& range) noexcept
{
  tbb::parallel_for(range, [&](auto const my_range) {
    auto const channel = tbb::this_task_arena::current_thread_index();
    graph.EdgeMap(f,
      subset,
      ranges::iota_view{ my_range.begin(), my_range.end() },
      channel);
  });
}


template<typename Graph, typename VertexProgram>
void EdgeMap(Graph& graph,
  VertexSubset const& subset,
  VertexProgram& f) noexcept
{
  EdgeMap(
    graph, subset, f, tbb::blocked_range<VertexLabel>{ 0, graph.max_v() + 1 });
}

template<typename Graph, typename VertexFunction>
void VertexMap(Graph& graph,
  VertexFunction const& f,
  famgraph::VertexRange range) noexcept
{
  tbb::parallel_for(
    tbb::blocked_range<VertexLabel>{ range.start, range.end_exclusive },
    [&](auto const& my_range) {
      for (auto v = my_range.begin(); v < my_range.end(); ++v) {
        f(graph[v], v);
      }
    });
}

template<typename Graph, typename VertexFunction>
void VertexMap(Graph& graph, VertexFunction const& f) noexcept
{
  auto const max_v = graph.max_v();
  VertexMap(graph, f, { 0, max_v + 1 });
}
}// namespace famgraph

#endif//__FAMGRAPH_H__
