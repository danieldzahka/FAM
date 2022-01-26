#ifndef __FAMGRAPH_H__
#define __FAMGRAPH_H__

#include <memory>
#include <cstdint>
#include <fgidx.hpp>
#include <FAM.hpp>

namespace famgraph {

struct VertexRange
{
  uint32_t const start;
  uint32_t const end;
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
  class Iterator
  {
    VertexRange const range_;
    uint32_t current_vertex_;
    RemoteGraph const &graph_;
    uint32_t *buffer;
    uint32_t volatile *cursor;

  public:
    Iterator(const VertexRange &range, RemoteGraph const &graph, int channel);

    bool HasNext() const noexcept;
    AdjacencyList Next() noexcept;
  };
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

public:
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

}// namespace famgraph

#endif//__FAMGRAPH_H__
