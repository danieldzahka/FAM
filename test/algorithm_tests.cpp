#include <catch2/catch.hpp>
#include <fmt/core.h>

#include <constants.hpp>
#include <famgraph.hpp>
#include <famgraph_algorithms.hpp>

namespace {
auto INPUTS_DIR = TEST_GRAPH_DIR;
auto const memserver_grpc_addr = MEMADDR;
auto const ipoib_addr = "192.168.12.2";
auto const ipoib_port = "35287";
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
}