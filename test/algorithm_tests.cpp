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
using namespace std::literals::string_view_literals;

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

auto constexpr small = "small/small"sv;
auto constexpr gnutella = "Gnutella04/p2p-Gnutella04"sv;
auto constexpr last_vert_nonempty = "last_vert_non_empty/graph"sv;
auto constexpr small_symmetric = "small-sym/small-sym"sv;
auto constexpr gnutella_symmetric = "Gnutella04-sym/p2p-Gnutella04-sym"sv;

const std::map<BfsKey, unsigned int> bfs_reference_output{ { { small, 0 }, 4 },
  { { gnutella, 0 }, 21 },
  { { last_vert_nonempty, 0 }, 3 } };

const std::map<KcoreKey, unsigned int> kcore_reference_output{
  { { small_symmetric, 2 }, 7 },
  { { gnutella_symmetric, 5 }, 5433 },
  { { gnutella_symmetric, 6 }, 4857 },
  { { gnutella_symmetric, 7 }, 365 },
};

template<typename AdjacencyGraph = famgraph::LocalGraph, typename... Args>
AdjacencyGraph CreateGraph(std::string_view graph_base, Args... args)
{
  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto index_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "idx");
  auto adjacency_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "adj");

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
  auto max_distance = bfs_reference_output.at({ graph_base, 0 });
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