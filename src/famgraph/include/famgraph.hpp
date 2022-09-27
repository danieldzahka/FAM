#ifndef __FAMGRAPH_H__
#define __FAMGRAPH_H__

#include <range/v3/all.hpp>
#include <memory>
#include <cstring>
#include <cstdint>
#include <fgidx.hpp>
#include <FAM.hpp>
#include <FAM_constants.hpp>

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

  static std::vector<famgraph::VertexRange> ConvertToRanges(
    VertexSubset const& vertex_subset) noexcept
  {
    return ConvertToRanges(vertex_subset, 0, vertex_subset.max_v_ + 1);
  }

  static std::vector<famgraph::VertexRange> ConvertToRanges(
    VertexSubset const& vertex_subset,
    VertexLabel start,
    VertexLabel end_exclusive) noexcept
  {
    std::vector<famgraph::VertexRange> ret;
    auto const range_start = start;
    auto const range_end_exclusive = end_exclusive;

    unsigned int i = range_start;
    while (i < range_end_exclusive) {
      if (vertex_subset[i]) {
        ret.push_back({ i, i });
        auto& current_range = ret.back();
        while (i < range_end_exclusive && vertex_subset[i]) {
          current_range.end_exclusive = ++i;
        }
      }
      ++i;
    }
    return ret;
  }

  uint32_t GetMaxV() const noexcept;
};

void PrintVertexSubset(VertexSubset const& vertex_subset) noexcept;

class RemoteGraph
{
  fgidx::DenseIndex const idx_;
  std::unique_ptr<FAM::FamControl> fam_control_;
  FAM::FamControl::RemoteRegion const adjacency_array_;
  FAM::FamControl::LocalRegion edge_window_;

  RemoteGraph(fgidx::DenseIndex&& idx,
    std::unique_ptr<FAM::FamControl>&& fam_control,
    FAM::FamControl::RemoteRegion adjacency_array,
    FAM::FamControl::LocalRegion edge_window);

  void PostSegmentsAndWait(std::vector<FAM::FamSegment> const& segments,
    std::uint32_t taken,
    int channel) noexcept;

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
  static famgraph::RemoteGraph CreateInstance(std::string const& index_file,
    std::string const& adj_file,
    std::string const& grpc_addr,
    std::string const& ipoib_addr,
    std::string const& ipoib_port,
    int rdma_channels);

  [[nodiscard]] uint32_t max_v() const noexcept;
  [[nodiscard]] EdgeIndexType Degree(VertexLabel v) const noexcept;

  struct Buffer
  {
    void *const p;
    uint64_t const length;
  };

  [[nodiscard]] Buffer GetChannelBuffer(int channel) const noexcept;

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
        for (unsigned int i = 0; i < num_edges; ++i) f(v, b[i], num_edges);
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

class LocalGraph
{
  fgidx::DenseIndex idx_;
  std::unique_ptr<uint32_t[]> adjacency_array_;

  LocalGraph(fgidx::DenseIndex&& idx,
    std::unique_ptr<uint32_t[]>&& adjacency_array);

public:
  static LocalGraph CreateInstance(std::string const& index_file,
    std::string const& adj_file);

  [[nodiscard]] uint32_t max_v() const noexcept;
  [[nodiscard]] EdgeIndexType Degree(VertexLabel v) const noexcept;

  template<typename Function, typename Filter>
  void EdgeMap(Function& f,
    Filter const& is_active,
    ranges::iota_view<std::uint32_t, std::uint32_t> range)
  {
    for (auto const v : range | ranges::views::filter(is_active)) {
      auto const [start_inclusive, end_exclusive] = this->idx_[v];
      auto const num_edges = end_exclusive - start_inclusive;
      auto const *edges = &this->adjacency_array_[start_inclusive];
      for (unsigned int i = 0; i < num_edges; ++i) {
        f(v, edges[i], num_edges);
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
};

template<typename Graph, typename VertexProgram>
void EdgeMap(Graph& graph,
  VertexSubset const& subset,
  VertexProgram& f) noexcept
{
  tbb::parallel_for(tbb::blocked_range<VertexLabel>{ 0, graph.max_v() + 1 },
    [&](auto const my_range) {
      auto const channel = tbb::this_task_arena::current_thread_index();
      graph.EdgeMap(f,
        subset,
        ranges::iota_view{ my_range.begin(), my_range.end() },
        channel);
    });
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
