#include <catch2/catch.hpp>
#include <fmt/core.h>
#include <vector>
#include <utility>
#include <fstream>

#include <constants.hpp>
#include <famgraph.hpp>
#include <unordered_set>

namespace {
auto INPUTS_DIR = TEST_GRAPH_DIR;
auto const memserver_grpc_addr = MEMADDR;
auto constexpr ipoib_addr = MEMSERVER_IPOIB;
auto constexpr ipoib_port = MEMSERVER_RDMA_PORT;

auto all_vertices = [](std::uint32_t) { return true; };
using Filter = std::function<bool(std::uint32_t)>;
auto CreateEdgeList(std::string const& plain_text_file, Filter f = all_vertices)
{
  std::ifstream ifs{ plain_text_file };
  std::vector<std::pair<uint32_t, uint32_t>> edge_list;
  uint32_t u, w;
  while (ifs >> u >> w) {
    if (f(u)) edge_list.emplace_back(std::make_pair(u, w));
  }

  return edge_list;
}

// Require: edge_list_a and edge_list_b are sorted
void CompareEdgeLists(
  std::vector<std::pair<uint32_t, uint32_t>> const& edge_list_a,
  std::vector<std::pair<uint32_t, uint32_t>> const& edge_list_b)
{
  REQUIRE(edge_list_a.size() == edge_list_b.size());
  for (unsigned long i = 0; i < edge_list_a.size(); ++i) {
    REQUIRE(edge_list_a[i] == edge_list_b[i]);
  }
}

template<typename AdjacencyGraph> struct GraphWrapper
{
  AdjacencyGraph graph;
  std::string_view name;
};

template<typename AdjacencyGraph = famgraph::LocalGraph<>, typename... Args>
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
}// namespace

TEST_CASE("LocalGraph Construction", "[local]")
{
  auto [graph, graph_base] = CreateGraph();
  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto const edge_list = CreateEdgeList(plain_text_edge_list);

  std::vector<std::pair<uint32_t, uint32_t>> edge_list2;
  auto build_edge_list = [&edge_list2](uint32_t const v,
                           uint32_t const w,
                           uint64_t const /*v_degree*/) noexcept {
    edge_list2.emplace_back(std::make_pair(v, w));
  };

  graph.EdgeMap(build_edge_list);
  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("RemoteGraph Construction", "[rdma]")
{
  int const rdma_channels = 1;
  auto [graph, graph_base] = CreateGraph<famgraph::RemoteGraph<>>(
    memserver_grpc_addr, ipoib_addr, ipoib_port, rdma_channels);

  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto const edge_list = CreateEdgeList(plain_text_edge_list);

  std::vector<std::pair<uint32_t, uint32_t>> edge_list2;
  auto build_edge_list = [&edge_list2](uint32_t const v,
                           uint32_t const w,
                           uint64_t const /*v_degree*/) noexcept {
    edge_list2.emplace_back(std::make_pair(v, w));
  };

  graph.EdgeMap(build_edge_list);
  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("LocalGraph Vertex Table", "[local]")
{
  int constexpr static magic = 123321;
  struct TestVertex
  {
    int value{ magic };
  };
  auto [local_graph, graph_base] = CreateGraph();
  auto graph =
    famgraph::Graph<TestVertex, famgraph::LocalGraph<>>{ local_graph };

  auto& vert = graph[0];
  REQUIRE(vert.value == magic);
}

TEST_CASE("Vertex Filter", "[local]")
{
  std::uint32_t constexpr max_v = (1 << 15) + 43534;
  famgraph::VertexSubset vertex_set{ max_v };

  for (std::uint32_t v = 0; v <= max_v; ++v) {
    REQUIRE(vertex_set[v] == false);
  }

  std::unordered_set<std::uint32_t> verts = { 1, 6623, 78, 96, max_v >> 1 };
  for (auto v : verts) { vertex_set.Set(v); }

  for (std::uint32_t v = 0; v <= max_v; ++v) {
    if (verts.count(v))
      REQUIRE(vertex_set[v]);
    else
      REQUIRE(!vertex_set[v]);
  }

  vertex_set.Clear();
  for (std::uint32_t v = 0; v <= max_v; ++v) {
    REQUIRE(vertex_set[v] == false);
  }
}

// TEST_CASE("Vertex Filter SetAll()", "[local]")
//{
//   std::uint32_t const max_v =
//     GENERATE((1U << 15U) + 43534U, 62U, 63U, 64U, 65U, 66U);
//   famgraph::VertexSubset vertex_set{ max_v };
//
//   vertex_set.SetAll();
//   auto const ranges = famgraph::VertexSubset::ConvertToRanges(vertex_set);
//   REQUIRE(ranges.size() == 1);
//   auto const range = ranges.front();
//   REQUIRE(range.start == 0);
//   REQUIRE(vertex_set[max_v]);
//   // REQUIRE(!vertex_set[max_v + 1]);
//   REQUIRE(range.end_exclusive == max_v + 1);
// }

namespace {
famgraph::VertexSubset RandomVertexSet(std::uint32_t n, std::mt19937& gen)
{
  std::bernoulli_distribution d(0.75);
  famgraph::VertexSubset ret{ n };
  for (std::uint32_t i = 0; i <= n; ++i) {
    if (d(gen)) ret.Set(i);
  }
  return ret;
}
}// namespace

TEST_CASE("Local Filter Edgemap", "[local]")
{
  auto [graph, graph_base] = CreateGraph();
  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");

  std::random_device rd;
  std::mt19937 gen(rd());
  auto vertex_subset = RandomVertexSet(graph.max_v(), gen);

  auto filter = [&vertex_subset](std::uint32_t v) { return vertex_subset[v]; };
  auto const edge_list = CreateEdgeList(plain_text_edge_list, filter);

  std::vector<std::pair<uint32_t, uint32_t>> edge_list2;
  auto build_edge_list = [&edge_list2](uint32_t const v,
                           uint32_t const w,
                           uint64_t const /*v_degree*/) noexcept {
    edge_list2.emplace_back(std::make_pair(v, w));
  };

  graph.EdgeMap(build_edge_list, vertex_subset);
  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("Local Filter Edgemap with Ranges", "[local]")
{
  auto [graph, graph_base] = CreateGraph();
  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");

  std::random_device rd;
  std::mt19937 gen(rd());
  auto vertex_subset = RandomVertexSet(graph.max_v(), gen);

  auto filter = [&](std::uint32_t v) { return vertex_subset[v]; };
  auto const edge_list = CreateEdgeList(plain_text_edge_list, filter);

  std::vector<std::pair<uint32_t, uint32_t>> edge_list2;
  auto build_edge_list = [&edge_list2](uint32_t const v,
                           uint32_t const w,
                           uint64_t const /*v_degree*/) noexcept {
    edge_list2.emplace_back(std::make_pair(v, w));
  };

  auto const end_exclusive = graph.max_v() + 1;
  auto const mid = end_exclusive / 2;

  graph.EdgeMap(build_edge_list, vertex_subset, { 0, mid });
  graph.EdgeMap(build_edge_list, vertex_subset, { mid, end_exclusive });
  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("Remote Filter Edgemap", "[rdma]")
{
  int const rdma_channels = 1;
  auto [graph, graph_base] = CreateGraph<famgraph::RemoteGraph<>>(
    memserver_grpc_addr, ipoib_addr, ipoib_port, rdma_channels);

  std::random_device rd;
  std::mt19937 gen(rd());
  auto vertex_subset = RandomVertexSet(graph.max_v(), gen);

  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto filter = [&vertex_subset](std::uint32_t v) { return vertex_subset[v]; };
  auto const edge_list = CreateEdgeList(plain_text_edge_list, filter);

  std::vector<std::pair<uint32_t, uint32_t>> edge_list2;
  auto build_edge_list = [&edge_list2](uint32_t const v,
                           uint32_t const w,
                           uint64_t const /*v_degree*/) noexcept {
    edge_list2.emplace_back(std::make_pair(v, w));
  };

  graph.EdgeMap(build_edge_list, vertex_subset);
  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("Remote Filter Edgemap with Ranges", "[rdma]")
{
  int const rdma_channels = 1;
  auto [graph, graph_base] = CreateGraph<famgraph::RemoteGraph<>>(
    memserver_grpc_addr, ipoib_addr, ipoib_port, rdma_channels);

  std::random_device rd;
  std::mt19937 gen(rd());
  auto vertex_subset = RandomVertexSet(graph.max_v(), gen);

  auto plain_text_edge_list =
    fmt::format("{}/{}.{}", INPUTS_DIR, graph_base, "txt");
  auto filter = [&vertex_subset](std::uint32_t v) { return vertex_subset[v]; };
  auto const edge_list = CreateEdgeList(plain_text_edge_list, filter);

  std::vector<std::pair<uint32_t, uint32_t>> edge_list2;
  auto build_edge_list = [&edge_list2](uint32_t const v,
                           uint32_t const w,
                           uint64_t const /*v_degree*/) noexcept {
    edge_list2.emplace_back(std::make_pair(v, w));
  };

  auto const end_exclusive = graph.max_v() + 1;
  auto const mid = end_exclusive / 2;

  graph.EdgeMap(build_edge_list, vertex_subset, { 0, mid });
  graph.EdgeMap(build_edge_list, vertex_subset, { mid, end_exclusive });
  CompareEdgeLists(edge_list, edge_list2);
}