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
auto const ipoib_addr = "192.168.12.2";
auto const ipoib_port = "35287";

auto all_vertices = [](std::uint32_t) { return true; };
using Filter = std::function<bool(std::uint32_t)>;
auto CreateEdgeList(std::string const &plain_text_file, Filter f = all_vertices)
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
  std::vector<std::pair<uint32_t, uint32_t>> const &edge_list_a,
  std::vector<std::pair<uint32_t, uint32_t>> const &edge_list_b)
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
}// namespace

TEST_CASE("LocalGraph Construction", "[famgraph]")
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

  famgraph::EdgeMapSequential(
    graph, famgraph::VertexRange{ 0, graph.max_v() + 1 }, build_edge_list);

  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("RemoteGraph Construction", "[famgraph]")
{
  int const rdma_channels = 1;
  auto [graph, graph_base] = CreateGraph<famgraph::RemoteGraph>(
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

  famgraph::EdgeMapSequential(
    graph, famgraph::VertexRange{ 0, graph.max_v() + 1 }, build_edge_list);

  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("LocalGraph Vertex Table", "[famgraph]")
{
  int constexpr static magic = 123321;
  struct TestVertex
  {
    int value{ magic };
  };
  auto [local_graph, graph_base] = CreateGraph();
  auto graph = famgraph::Graph<TestVertex, famgraph::LocalGraph>{ local_graph };

  auto &vert = graph[0];
  REQUIRE(vert.value == magic);
}

TEST_CASE("Vertex Filter")
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

TEST_CASE("Vertex Filter SetAll()")
{
  std::uint32_t const max_v = GENERATE((1 << 15) + 43534, 62, 63, 64, 65, 66);
  famgraph::VertexSubset vertex_set{ max_v };

  vertex_set.SetAll();
  auto const ranges = famgraph::VertexSubset::ConvertToRanges(vertex_set);
  REQUIRE(ranges.size() == 1);
  auto const range = ranges.front();
  REQUIRE(range.start == 0);
  REQUIRE(vertex_set[max_v]);
  // REQUIRE(!vertex_set[max_v + 1]);
  REQUIRE(range.end_exclusive == max_v + 1);
}

TEST_CASE("Convert Vertex Subset to Range")
{
  using vr = std::vector<famgraph::VertexRange>;

  std::uint32_t const max_v = 867530;
  auto ranges = GENERATE(vr{},
    vr{ { 1, 2 }, { 4, 5 } },
    vr{ { 2, 1035 } },
    vr{ { max_v, max_v + 1 } },
    vr{ { 1, 5 },
      { 6, 89 },
      { 1000, 2000 },
      { 56739, 64980 },
      { max_v, max_v + 1 } });

  famgraph::VertexSubset vertex_set{ max_v };

  for (auto const &range : ranges) {
    for (auto v = range.start; v < range.end_exclusive; ++v) {
      vertex_set.Set(v);
    }
  }

  auto ranges2 = famgraph::VertexSubset::ConvertToRanges(vertex_set);
  REQUIRE(ranges.size() == ranges2.size());
  for (unsigned long i = 0; i < ranges.size(); ++i) {
    REQUIRE(ranges[i].start == ranges2[i].start);
    REQUIRE(ranges[i].end_exclusive == ranges2[i].end_exclusive);
  }
}

// TODO:: Test needs to ensure non-overlapping ranges
TEST_CASE("Convert Vertex Set to Ranges with bounds")
{
  using vr = std::vector<famgraph::VertexRange>;

  std::uint32_t const max_v = 867530;
  auto ranges = GENERATE(vr{},
    vr{ { 1, 2 }, { 4, 5 } },
    vr{ { 2, 1035 } },
    vr{ { max_v, max_v + 1 } },
    vr{ { 1, 5 },
      { 6, 89 },
      { 1000, 2000 },
      { 56739, 64980 },
      { max_v, max_v + 1 } });

  famgraph::VertexSubset vertex_set{ max_v };

  for (auto const &range : ranges) {
    for (auto v = range.start; v < range.end_exclusive; ++v) {
      vertex_set.Set(v);
    }
  }

  famgraph::VertexSubset comparison_set{ max_v };

  const unsigned int slice_size = 23;
  auto const slices = std::max(1U, (max_v + 1) / slice_size);
  for (unsigned int i = 0; i < slices; ++i) {
    auto const start = i * slice_size;
    auto const end_exclusive = i == slices - 1 ? max_v + 1 : start + slice_size;
    auto const my_ranges =
      famgraph::VertexSubset::ConvertToRanges(vertex_set, start, end_exclusive);
    for (auto const &range : my_ranges) {
      for (auto ii = range.start; ii < range.end_exclusive; ++ii) {
        comparison_set.Set(ii);
      }
    }
  }

  for (famgraph::VertexLabel v = 0; v <= max_v; ++v) {
    REQUIRE(comparison_set[v] == vertex_set[v]);
  }
}

namespace {
famgraph::VertexSubset RandomVertexSet(std::uint32_t n, std::mt19937 &gen)
{
  std::bernoulli_distribution d(0.75);
  famgraph::VertexSubset ret{ n };
  for (std::uint32_t i = 0; i <= n; ++i) {
    if (d(gen)) ret.Set(i);
  }
  return ret;
}
}// namespace

TEST_CASE("Local Filter Edgemap")
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

  famgraph::EdgeMapSequential(graph, vertex_subset, build_edge_list);

  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("Local Filter Edgemap with Ranges")
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

  auto const end_exclusive = graph.max_v() + 1;
  auto const mid = end_exclusive / 2;

  famgraph::PrintVertexSubset(vertex_subset);

  auto subset = famgraph::VertexSubset::ConvertToRanges(vertex_subset, 0, mid);
  famgraph::EdgeMapSequential(graph, subset, build_edge_list);

  auto subset2 =
    famgraph::VertexSubset::ConvertToRanges(vertex_subset, mid, end_exclusive);
  famgraph::EdgeMapSequential(graph, subset2, build_edge_list);

  CompareEdgeLists(edge_list, edge_list2);
}

TEST_CASE("Remote Filter Edgemap")
{
  int const rdma_channels = 1;
  auto [graph, graph_base] = CreateGraph<famgraph::RemoteGraph>(
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

  famgraph::EdgeMapSequential(graph, vertex_subset, build_edge_list);

  CompareEdgeLists(edge_list, edge_list2);
}