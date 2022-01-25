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

  RemoteGraph(fgidx::DenseIndex &&idx,
    std::unique_ptr<FAM::FamControl> &&fam_control,
    FAM::FamControl::RemoteRegion adjacency_array);

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

  public:
    Iterator(const VertexRange &range);

    bool HasNext();
    AdjacencyList Next();
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

  uint32_t max_v();

public:
  class Iterator
  {
    VertexRange const range_;
    uint32_t current_vertex_;
    LocalGraph const &graph_;

  public:
    Iterator(const VertexRange &range, LocalGraph const &graph);

    bool HasNext();
    AdjacencyList Next();
  };

  Iterator GetIterator(VertexRange const& range);
};

}// namespace famgraph

#endif//__FAMGRAPH_H__
