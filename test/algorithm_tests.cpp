#include <catch2/catch.hpp>
#include <fmt/core.h>

#include <map>
#include <string_view>

#include <constants.hpp>
#include <famgraph.hpp>
#include <famgraph_algorithms.hpp>

namespace {
using namespace std::literals::string_view_literals;

auto constexpr INPUTS_DIR = TEST_GRAPH_DIR;
auto constexpr LARGE_INPUTS_DIR = LARGE_TEST_GRAPH_DIR;
auto constexpr memserver_grpc_addr = MEMADDR;
auto constexpr ipoib_addr = "192.168.12.2";
auto constexpr ipoib_port = "35287";

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

auto constexpr small = TEST_GRAPH_DIR "/small/small"sv;
auto constexpr gnutella = TEST_GRAPH_DIR "/Gnutella04/p2p-Gnutella04"sv;
auto constexpr last_vert_nonempty =
  TEST_GRAPH_DIR "/last_vert_non_empty/graph"sv;
auto constexpr twitter7 = LARGE_TEST_GRAPH_DIR "/twitter7"sv;

auto constexpr small_symmetric = TEST_GRAPH_DIR "/small-sym/small-sym"sv;
auto constexpr gnutella_symmetric =
  TEST_GRAPH_DIR "/Gnutella04-sym/p2p-Gnutella04-sym"sv;
auto constexpr twitter7_symmetric =
  LARGE_TEST_GRAPH_DIR "/twitter7-undirected"sv;

const std::map<BfsKey, unsigned int> bfs_reference_output{ { { small, 0 }, 4 },
  { { gnutella, 0 }, 21 },
  { { last_vert_nonempty, 0 }, 3 },
  { { twitter7_symmetric, 1 }, 14 } };

const std::map<KcoreKey, unsigned int> kcore_reference_output{
  { { small_symmetric, 2 }, 7 },
  { { gnutella_symmetric, 5 }, 5433 },
  { { gnutella_symmetric, 6 }, 4857 },
  { { gnutella_symmetric, 7 }, 365 },
  { { twitter7_symmetric, 100 }, 2264960 }
};

struct ConnectedComponentsResult
{
  famgraph::VertexLabel total_components;
  famgraph::VertexLabel non_trivial_components;
  famgraph::VertexLabel largest_component_size;
};
const std::map<std::string_view, ConnectedComponentsResult>
  connected_components_output{
    { small_symmetric, { 6, 3, 7 } },
    { gnutella_symmetric, { 4, 1, 10876 } },
    { twitter7_symmetric, { 2, 1, 41652230 } },
  };

template<typename AdjacencyGraph = famgraph::LocalGraph, typename... Args>
AdjacencyGraph CreateGraph(std::string_view graph_base, Args... args)
{
  auto index_file = fmt::format("{}.{}", graph_base, "idx");
  auto adjacency_file = fmt::format("{}.{}", graph_base, "adj");

  return { AdjacencyGraph::CreateInstance(
    index_file, adjacency_file, args...) };
}

template<typename Graph>
void RunBFS(Graph &graph,
  std::string_view graph_base,
  std::uint32_t start_vertex)
{
  auto breadth_first_search = famgraph::BreadthFirstSearch(graph);
  auto result = breadth_first_search(start_vertex);
  auto max_distance = bfs_reference_output.at({ graph_base, start_vertex });
  REQUIRE(result.max_distance == max_distance);
}

template<typename Graph>
void RunKcore(Graph &graph, std::string_view graph_base, std::uint32_t kcore_k)
{
  auto kcore_decomposition = famgraph::KcoreDecomposition(graph);
  auto result = kcore_decomposition(kcore_k);
  auto kth_core_size = kcore_reference_output.at({ graph_base, kcore_k });
  REQUIRE(result.kth_core_membership == kth_core_size);
}

template<typename Graph>
void RunConnectedComponents(Graph &graph, std::string_view graph_base)
{
  auto connected_components = famgraph::ConnectedComponents(graph);
  auto result = connected_components();
  auto reference = connected_components_output.at(graph_base);
  REQUIRE(result.components == reference.total_components);
  REQUIRE(result.non_trivial_components == reference.non_trivial_components);
  REQUIRE(result.largest_component_size == reference.largest_component_size);
}

template<typename Graph>
void RunPageRank(Graph &graph, std::string_view graph_base)
{
  auto page_rank = famgraph::PageRank(graph);
  auto result = page_rank();
  fmt::print("Pagerank Iterations {}\n", result.iterations);
  for (auto const &pair : result.topN) {
    fmt::print("vertex: {}, value: {}\n", pair.second, pair.first);
  }
  //  auto reference = connected_components_output.at(graph_base);
  //  REQUIRE(result.components == reference.total_components);
  //  REQUIRE(result.non_trivial_components ==
  //  reference.non_trivial_components); REQUIRE(result.largest_component_size
  //  == reference.largest_component_size);
}
}// namespace

TEST_CASE("LocalGraph Breadth First Search")
{
  auto [graph_base, start_vertex] = GENERATE(
    BfsKey{ small, 0 }, BfsKey{ gnutella, 0 }, BfsKey{ last_vert_nonempty, 0 });
  auto graph = CreateGraph(graph_base);
  RunBFS(graph, graph_base, start_vertex);
}

TEST_CASE("RemoteGraph Breadth First Search")
{
  auto [graph_base, start_vertex] = GENERATE(
    BfsKey{ small, 0 }, BfsKey{ gnutella, 0 }, BfsKey{ last_vert_nonempty, 0 });
  int const rdma_channels = 1;
  auto graph = CreateGraph<famgraph::RemoteGraph>(
    graph_base, memserver_grpc_addr, ipoib_addr, ipoib_port, rdma_channels);
  RunBFS(graph, graph_base, start_vertex);
}

TEST_CASE("LocalGraph Kcore Decomposition")
{
  auto [graph_base, k] = GENERATE(KcoreKey{ small_symmetric, 2 },
    KcoreKey{ gnutella_symmetric, 5 },
    KcoreKey{ gnutella_symmetric, 6 },
    KcoreKey{ gnutella_symmetric, 7 });

  auto graph = CreateGraph(graph_base);
  RunKcore(graph, graph_base, k);
}

TEST_CASE("RemoteGraph Kcore Decomposition")
{
  auto [graph_base, k] = GENERATE(KcoreKey{ small_symmetric, 2 },
    KcoreKey{ gnutella_symmetric, 5 },
    KcoreKey{ gnutella_symmetric, 6 },
    KcoreKey{ gnutella_symmetric, 7 });

  int const rdma_channels = 1;
  auto graph = CreateGraph<famgraph::RemoteGraph>(
    graph_base, memserver_grpc_addr, ipoib_addr, ipoib_port, rdma_channels);
  RunKcore(graph, graph_base, k);
}

TEST_CASE("LocalGraph ConnectedComponents")
{
  auto graph_base = GENERATE(small_symmetric, gnutella_symmetric);
  auto graph = CreateGraph(graph_base);
  RunConnectedComponents(graph, graph_base);
}

TEST_CASE("RemoteGraph ConnectedComponents")
{
  auto graph_base = GENERATE(small_symmetric, gnutella_symmetric);
  int const rdma_channels = 1;
  auto graph = CreateGraph<famgraph::RemoteGraph>(
    graph_base, memserver_grpc_addr, ipoib_addr, ipoib_port, rdma_channels);
  RunConnectedComponents(graph, graph_base);
}

TEST_CASE("LocalGraph PageRank")
{
  auto graph_base = GENERATE(gnutella);
  auto graph = CreateGraph(graph_base);
  RunPageRank(graph, graph_base);
}

TEST_CASE("RemoteGraph PageRank")
{
  auto graph_base = GENERATE(gnutella);
  int const rdma_channels = 1;
  auto graph = CreateGraph<famgraph::RemoteGraph>(
    graph_base, memserver_grpc_addr, ipoib_addr, ipoib_port, rdma_channels);
  RunPageRank(graph, graph_base);
}

TEST_CASE("Large Graph LocalGraph Breadth First Search")
{
  auto [graph_base, start_vertex] = GENERATE(BfsKey{ twitter7_symmetric, 1 });
  auto graph = CreateGraph(graph_base);
  RunBFS(graph, graph_base, start_vertex);
}

TEST_CASE("Large Graph LocalGraph Kcore Decomposition")
{
  auto [graph_base, k] = GENERATE(KcoreKey{ twitter7_symmetric, 100 });
  auto graph = CreateGraph(graph_base);
  RunKcore(graph, graph_base, k);
}

TEST_CASE("Large Graph LocalGraph ConnectedComponents")
{
  auto graph_base = GENERATE(twitter7_symmetric);
  auto graph = CreateGraph(graph_base);
  RunConnectedComponents(graph, graph_base);
}