#include <catch2/catch.hpp>
#include <fmt/core.h>

#include <map>
#include <string_view>

#include <constants.hpp>
#include <famgraph.hpp>
#include <famgraph_algorithms.hpp>

namespace {
auto INPUTS_DIR = TEST_GRAPH_DIR;
auto const memserver_grpc_addr = MEMADDR;
auto const ipoib_addr = "192.168.12.2";
auto const ipoib_port = "35287";
}// namespace

namespace {

struct BfsKey
{
  std::string_view graph_name;
  std::uint32_t start_vertex;
  bool operator==(const BfsKey &rhs) const
  {
    return graph_name == rhs.graph_name && start_vertex == rhs.start_vertex;
  }
  bool operator<(const BfsKey &rhs) const
  {
    if (graph_name < rhs.graph_name) return true;
    if (rhs.graph_name < graph_name) return false;
    return start_vertex < rhs.start_vertex;
  }
};

struct KcoreKey
{
  std::string_view graph_name;
  std::uint32_t kth_core_size;
  bool operator==(const KcoreKey &rhs) const
  {
    return graph_name == rhs.graph_name && kth_core_size == rhs.kth_core_size;
  }
  bool operator<(const KcoreKey &rhs) const
  {
    if (graph_name < rhs.graph_name) return true;
    if (rhs.graph_name < graph_name) return false;
    return kth_core_size < rhs.kth_core_size;
  }
};

using namespace std::literals::string_view_literals;
const std::map<BfsKey, unsigned int> bfs_reference_output{
  { { "small/small"sv, 0 }, 4 },
  { { "Gnutella04/p2p-Gnutella04"sv, 0 }, 21 },
  { { "last_vert_non_empty/graph"sv, 0 }, 3 }
};
const std::map<KcoreKey, unsigned int> kcore_reference_output{
  { { "small/small"sv, 0 }, 4 },
  { { "Gnutella04/p2p-Gnutella04"sv, 0 }, 21 },
};
template<typename AdjacencyGraph> struct GraphWrapper
{
  AdjacencyGraph graph;
  std::string_view name;
};

template<typename AdjacencyGraph = famgraph::LocalGraph, typename... Args>
GraphWrapper<AdjacencyGraph> CreateGraph(Args... args)
{
  auto graph_base = GENERATE(
    "small/small", "Gnutella04/p2p-Gnutella04", "last_vert_non_empty/graph");
  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto index_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "idx");
  auto adjacency_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "adj");

  return { AdjacencyGraph::CreateInstance(index_file, adjacency_file, args...),
    graph_base };
}

template<typename Graph> void RunBFS(Graph &graph, std::string_view graph_base)
{
  auto breadth_first_search = famgraph::BreadthFirstSearch(graph);
  std::uint32_t const start_vertex = 0;
  auto result = breadth_first_search(start_vertex);
  auto max_distance = bfs_reference_output.at({ graph_base, 0 });
  REQUIRE(result.max_distance == max_distance);
}

template<typename Graph>
void RunKcore(Graph &graph, std::string_view graph_base)
{
  auto breadth_first_search = famgraph::BreadthFirstSearch(graph);
  std::uint32_t const kcore_k = 100;
  auto result = breadth_first_search(kcore_k);
  auto kth_core_size = kcore_reference_output.at({ graph_base, kcore_k });
  REQUIRE(result.max_distance == kth_core_size);
}
}// namespace

TEST_CASE("LocalGraph Breadth First Search")
{
  auto [graph, graph_base] = CreateGraph();
  auto breadth_first_search = famgraph::BreadthFirstSearch(graph);
  RunBFS(graph, graph_base);
}

TEST_CASE("RemoteGraph Breadth First Search")
{
  int const rdma_channels = 1;
  auto [graph, graph_base] = CreateGraph<famgraph::RemoteGraph>(
    memserver_grpc_addr, ipoib_addr, ipoib_port, rdma_channels);
  RunBFS(graph, graph_base);
}