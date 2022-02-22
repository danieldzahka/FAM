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

using namespace std::literals::string_view_literals;
const std::map<BfsKey, unsigned int> bfs_reference_output{
  { { "small/small"sv, 0 }, 4 },
  { { "Gnutella04/p2p-Gnutella04"sv, 0 }, 21 },
  { { "last_vert_non_empty/graph"sv, 0 }, 3 }
};
}// namespace

TEST_CASE("Breadth First Search")
{
  auto graph_base = GENERATE(
    "small/small", "Gnutella04/p2p-Gnutella04", "last_vert_non_empty/graph");
  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto index_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "idx");
  auto adjacency_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "adj");

  auto local_graph =
    famgraph::LocalGraph::CreateInstance(index_file, adjacency_file);

  auto breadth_first_search = famgraph::BreadthFirstSearch(local_graph);
  std::uint32_t const start_vertex = 0;
  auto result = breadth_first_search(start_vertex);
  auto max_distance = bfs_reference_output.at({ graph_base, 0 });
  REQUIRE(result.max_distance == max_distance);
}