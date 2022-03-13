#ifndef __FAMGRAPH_H__
#define __FAMGRAPH_H__

#include <memory>
#include <cstring>
#include <cstdint>
#include <fgidx.hpp>
#include <FAM.hpp>

#include <oneapi/tbb.h>

namespace famgraph {
using VertexLabel = std::uint32_t;
using EdgeIndexType = std::uint64_t;
constexpr uint32_t null_vert = std::numeric_limits<std::uint32_t>::max();

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
    auto &word = this->bitmap_[Offset(v)];
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
    VertexSubset const &vertex_subset) noexcept
  {
    return ConvertToRanges(vertex_subset, 0, vertex_subset.max_v_ + 1);
  }

  static std::vector<famgraph::VertexRange> ConvertToRanges(
    VertexSubset const &vertex_subset,
    VertexLabel start,
    VertexLabel end_exclusive) noexcept
  {
    std::vector<famgraph::VertexRange> ret;
    const auto range_start = start;
    const auto range_end_exclusive = end_exclusive;

    unsigned int i = range_start;
    while (i < range_end_exclusive) {
      if (vertex_subset[i]) {
        ret.push_back({ i, i });
        auto &current_range = ret.back();
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

void PrintVertexSubset(VertexSubset const &vertex_subset) noexcept;

class RemoteGraph
{
  fgidx::DenseIndex const idx_;
  std::unique_ptr<FAM::FamControl> fam_control_;
  FAM::FamControl::RemoteRegion const adjacency_array_;
  FAM::FamControl::LocalRegion edge_window_;

  RemoteGraph(fgidx::DenseIndex &&idx,
    std::unique_ptr<FAM::FamControl> &&fam_control,
    FAM::FamControl::RemoteRegion adjacency_array,
    FAM::FamControl::LocalRegion edge_window);

public:
  static famgraph::RemoteGraph CreateInstance(std::string const &index_file,
    std::string const &adj_file,
    std::string const &grpc_addr,
    std::string const &ipoib_addr,
    std::string const &ipoib_port,
    int rdma_channels);

  [[nodiscard]] uint32_t max_v() const noexcept;
  [[nodiscard]] EdgeIndexType Degree(VertexLabel v) const noexcept;

  struct Buffer
  {
    void *const p;
    uint64_t const length;
  };

  [[nodiscard]] Buffer GetChannelBuffer(int channel) const noexcept;

  class Iterator
  {
    std::vector<VertexRange> const ranges_;
    decltype(ranges_.begin()) current_range_;
    VertexRange current_window_;
    uint32_t current_vertex_;
    RemoteGraph const &graph_;
    Buffer edge_buffer_;
    uint32_t *cursor;
    int const channel_;

    VertexRange MaximalRange(uint32_t range_start) noexcept;
    void FillWindow(VertexRange range) noexcept;

  public:
    Iterator(std::vector<VertexRange> &&ranges,
      RemoteGraph const &graph,
      int channel);

    [[nodiscard]] bool HasNext() noexcept;
    AdjacencyList Next() noexcept;
  };

  // TODO: Iterator constructor should take const & ranges
  [[nodiscard]] Iterator GetIterator(std::vector<VertexRange> ranges,
    int channel = 0) const noexcept
  {
    return Iterator(std::move(ranges), *this, channel);
  }
  [[nodiscard]] Iterator GetIterator(VertexRange const &range,
    int channel = 0) const noexcept;
  [[nodiscard]] Iterator GetIterator(VertexSubset const &vertex_set,
    int channel = 0) const noexcept;
};

class LocalGraph
{
  fgidx::DenseIndex idx_;
  std::unique_ptr<uint32_t[]> adjacency_array_;

  LocalGraph(fgidx::DenseIndex &&idx,
    std::unique_ptr<uint32_t[]> &&adjacency_array);

public:
  static LocalGraph CreateInstance(std::string const &index_file,
    std::string const &adj_file);

  [[nodiscard]] uint32_t max_v() const noexcept;
  [[nodiscard]] EdgeIndexType Degree(VertexLabel v) const noexcept;

  class Iterator
  {
    std::vector<VertexRange> const ranges_;
    decltype(ranges_.begin()) current_range_;
    uint32_t current_vertex_;
    LocalGraph const &graph_;

  public:
    Iterator(std::vector<VertexRange> &&ranges, LocalGraph const &graph);

    bool HasNext() noexcept;
    AdjacencyList Next() noexcept;
  };

  // TODO: Iterator constructor should take const & ranges
  [[nodiscard]] Iterator GetIterator(
    std::vector<VertexRange> ranges) const noexcept
  {
    return Iterator(std::move(ranges), *this);
  }

  [[nodiscard]] Iterator GetIterator(VertexRange const &range) const noexcept;
  [[nodiscard]] Iterator GetIterator(
    VertexSubset const &vertex_set) const noexcept;
};

template<typename Vertex, typename AdjancencyGraph> class Graph
{
  AdjancencyGraph &adjacency_graph_;
  std::unique_ptr<Vertex[]> vertex_array_;

public:
  explicit Graph(AdjancencyGraph &adjacency_graph)
    : adjacency_graph_(adjacency_graph),
      vertex_array_(new Vertex[adjacency_graph_.max_v() + 1])
  {}

  auto max_v() const noexcept { return this->adjacency_graph_.max_v(); }
  [[nodiscard]] auto Degree(VertexLabel v) const noexcept
  {
    return this->adjacency_graph_.Degree(v);
  };

  Vertex &operator[](std::uint32_t v) noexcept
  {
    return this->vertex_array_[v];
  }

  AdjancencyGraph &getAdjacencyGraph() const noexcept
  {
    return adjacency_graph_;
  }
};

template<typename Graph, typename VertexSet, typename VertexProgram>
void EdgeMapSequential(Graph &graph,
  VertexSet const &vertex_subset,
  VertexProgram &f) noexcept
{
  auto iterator = graph.GetIterator(vertex_subset);
  while (iterator.HasNext()) {
    auto const [v, n, edges] = iterator.Next();
    for (unsigned long i = 0; i < n; ++i) f(v, edges[i], n);
  }
}

template<typename Graph, typename VertexProgram>
void EdgeMap(Graph &graph,
  VertexSubset const &subset,
  VertexProgram &f) noexcept
{
  tbb::parallel_for(tbb::blocked_range<VertexLabel>{ 0, graph.max_v() + 1 },
    [&](auto const my_range) {
      auto const rs =
        VertexSubset::ConvertToRanges(subset, my_range.begin(), my_range.end());
      EdgeMapSequential(graph, rs, f);
    });
}

template<typename Graph, typename VertexFunction>
void VertexMap(Graph &graph,
  VertexFunction const &f,
  famgraph::VertexRange range) noexcept
{
  tbb::parallel_for(
    tbb::blocked_range<VertexLabel>{ range.start, range.end_exclusive },
    [&](auto const &my_range) {
      for (auto v = my_range.begin(); v < my_range.end(); ++v) {
        f(graph[v], v);
      }
    });
}

template<typename Graph, typename VertexFunction>
void VertexMap(Graph &graph, VertexFunction const &f) noexcept
{
  auto const max_v = graph.max_v();
  VertexMap(graph, f, { 0, max_v + 1 });
}
}// namespace famgraph

#endif//__FAMGRAPH_H__
