#ifndef __FAMGRAPH_H__
#define __FAMGRAPH_H__

#include <memory>
#include <cstring>
#include <cstdint>
#include <fgidx.hpp>
#include <FAM.hpp>

namespace famgraph {

constexpr uint32_t null_vert = 0xFFFFFFFF;

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

  uint32_t max_v() const noexcept;

  struct Buffer
  {
    void *const p;
    uint64_t const length;
  };

  Buffer GetChannelBuffer(int channel) const noexcept;

  class Iterator
  {
    VertexRange const range_;
    VertexRange current_window_;
    uint32_t current_vertex_;
    RemoteGraph const &graph_;
    Buffer edge_buffer_;
    uint32_t *cursor;
    int const channel_;

    VertexRange MaximalRange(uint32_t range_start) noexcept;
    void FillWindow(VertexRange range) noexcept;

  public:
    Iterator(const VertexRange &range, RemoteGraph const &graph, int channel);

    bool HasNext() const noexcept;
    AdjacencyList Next() noexcept;
  };

  Iterator GetIterator(VertexRange const &range,
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

  uint32_t max_v() const noexcept;

  class Iterator
  {
    VertexRange const range_;
    uint32_t current_vertex_;
    LocalGraph const &graph_;

  public:
    Iterator(const VertexRange &range, LocalGraph const &graph);

    bool HasNext() const noexcept;
    AdjacencyList Next() noexcept;
  };

  Iterator GetIterator(VertexRange const &range) const noexcept;
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

  Vertex &operator[](std::uint32_t v) noexcept
  {
    return this->vertex_array_[v];
  }
};

template<typename Graph, typename VertexProgram>
void EdgeMap(Graph const &graph,
  VertexProgram const &f,
  VertexRange const &range) noexcept
{
  auto iterator = graph.GetIterator(range);
  while (iterator.HasNext()) {
    auto const [v, n, edges] = iterator.Next();
    for (unsigned long i = 0; i < n; ++i) f(v, edges[i], n);
  }
}

class VertexSubset
{
  std::unique_ptr<std::uint64_t[]> bitmap_;
  std::uint32_t const max_v_;
  std::uint32_t size{ 0 };

  constexpr static std::uint32_t Offset(std::uint32_t v) { return v >> 6; }
  constexpr static std::uint32_t BitOffset(std::uint32_t v)
  {
    return v & ((1 << 6) - 1);
  }

public:
  VertexSubset(uint32_t max_v);

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
    if (was_unset) this->size++;
    return was_unset;
  }

  bool IsEmpty() const noexcept { return this->size == 0; }
  void Clear() noexcept
  {
    std::memset(this->bitmap_.get(),
      0,
      sizeof(std::uint64_t) * (1 + Offset(this->max_v_)));
  }
};

}// namespace famgraph

#endif//__FAMGRAPH_H__
