#include <catch2/catch.hpp>
#include <fmt/core.h>
#include <vector>
#include <utility>
#include <fstream>

#include <constants.hpp>
#include <famgraph.hpp>

namespace {
auto INPUTS_DIR = TEST_GRAPH_DIR;
auto const memserver_grpc_addr = MEMADDR;
auto const ipoib_addr = "192.168.12.2";
auto const ipoib_port = "35287";

auto CreateEdgeList(std::string const &plain_text_file)
{
  std::ifstream ifs{ plain_text_file };
  std::vector<std::pair<uint32_t, uint32_t>> edge_list;
  uint32_t u, w;
  while (ifs >> u >> w) { edge_list.emplace_back(std::make_pair(u, w)); }

  return edge_list;
}

// Require: edge_list_a and edge_list_b are sorted
void CompareEdgeLists(
  std::vector<std::pair<uint32_t, uint32_t>> const &edge_list_a,
  std::vector<std::pair<uint32_t, uint32_t>> const &edge_list_b)
{
  REQUIRE(edge_list_a.size() == edge_list_b.size());
  for (unsigned long i = 0; i < edge_list_a.size(); ++i) {
    REQUIRE(edge_list_a[i] == edge_list_b[i]);
  }
}

}// namespace

TEST_CASE("LocalGraph Construction", "[famgraph]")
{
  auto graph_base = GENERATE(
    "small/small", "Gnutella04/p2p-Gnutella04", "last_vert_non_empty/graph");
  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto index_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "idx");
  auto adjacency_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "adj");
  auto const edge_list = CreateEdgeList(plain_text_edge_list);

  auto graph = famgraph::LocalGraph::CreateInstance(index_file, adjacency_file);
  std::vector<std::pair<uint32_t, uint32_t>> edge_list2;
  auto build_edge_list = [&edge_list2](uint32_t const v,
                           uint32_t const w,
                           uint64_t const /*v_degree*/) noexcept {
    edge_list2.emplace_back(std::make_pair(v, w));
  };

  famgraph::EdgeMap(
    graph, build_edge_list, famgraph::VertexRange{ 0, graph.max_v() + 1 });

  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("RemoteGraph Construction", "[famgraph]")
{
  auto graph_base = GENERATE(
    "small/small", "Gnutella04/p2p-Gnutella04", "last_vert_non_empty/graph");
  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto index_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "idx");
  auto adjacency_file = fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "adj");
  int const rdma_channels = 1;

  auto const edge_list = CreateEdgeList(plain_text_edge_list);

  auto graph = famgraph::RemoteGraph::CreateInstance(index_file,
    adjacency_file,
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
}