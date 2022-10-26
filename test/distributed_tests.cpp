#include <fmt/core.h>
#include <fmt/color.h>
#include <map>
#include <string_view>

#include <constants.hpp>
#include <famgraph.hpp>
#include <famgraph_algorithms.hpp>
#include <mpi_substrate.hpp>

namespace {
using namespace std::literals::string_view_literals;

auto constexpr INPUTS_DIR = TEST_GRAPH_DIR;
auto constexpr LARGE_INPUTS_DIR = LARGE_TEST_GRAPH_DIR;
auto constexpr memserver_grpc_addr = MEMADDR;
auto constexpr ipoib_addr = MEMSERVER_IPOIB;
auto constexpr ipoib_port = MEMSERVER_RDMA_PORT;

static constexpr int threads = 5;

struct BfsKey
{
  std::string_view graph_name;
  std::uint32_t start_vertex;
  bool operator==(BfsKey const& rhs) const
  {
    return graph_name == rhs.graph_name && start_vertex == rhs.start_vertex;
  }
  bool operator<(BfsKey const& rhs) const
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
  bool operator==(KcoreKey const& rhs) const
  {
    return graph_name == rhs.graph_name && kth_core_size == rhs.kth_core_size;
  }
  bool operator<(KcoreKey const& rhs) const
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
  { { last_vert_nonempty, 0 }, 2 },
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

struct PageRankResult
{
  int iterations;
  float tolerance;
  std::vector<std::pair<famgraph::VertexLabel, float>> top20;
};

const std::map<std::string_view, PageRankResult> pagerank_output{
  { twitter7,
    { 94,
      .001f,
      {
        { 41600, 14024.1 },
        { 911417, 11151.9 },
        { 1605291, 9421.92 },
        { 15692599, 8328.66 },
        { 2583597, 7921.08 },
        { 271355, 6621.46 },
        { 798058, 6523.61 },
        { 677144, 4990.21 },
        { 145987, 4752.83 },
        { 3654935, 3130.98 },
        { 11379732, 2830.07 },
        { 142024, 2810.65 },
        { 1040165, 2793.47 },
        { 620972, 2760.91 },
        { 5694562, 2736.49 },
        { 1252091, 2665.83 },
        { 172942, 2656.16 },
        { 669889, 2644.37 },
        { 2699568, 2574.65 },
        { 1071824, 2557.81 },
      } } },
  { gnutella,
    { 14,
      1.0f,
      {
        { 1056, 1.82374 },
        { 1054, 1.80341 },
        { 1536, 1.49605 },
        { 171, 1.47888 },
        { 453, 1.42508 },
        { 407, 1.38735 },
        { 263, 1.3825 },
        { 4664, 1.36403 },
        { 1959, 1.32756 },
        { 261, 1.32281 },
        { 410, 1.31858 },
        { 165, 1.31745 },
        { 1198, 1.25505 },
        { 127, 1.221 },
        { 4054, 1.18981 },
        { 2265, 1.17525 },
        { 345, 1.17203 },
        { 763, 1.17117 },
        { 989, 1.14456 },
        { 987, 1.13871 },
      } } }
};

void REQUIRE(bool ok)
{
  if (ok) {
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "PASS\n");
  } else {
    fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "TEST FAILED\n");
    throw std::runtime_error(fmt::format("Test Failed\n"));
  }
}

template<typename AdjacencyGraph = famgraph::LocalGraph, typename... Args>
AdjacencyGraph CreateGraph(std::string_view graph_base, Args... args)
{
  auto index_file = fmt::format("{}.{}", graph_base, "idx");
  auto adjacency_file = fmt::format("{}.{}", graph_base, "adj");

  return { AdjacencyGraph::CreateInstance(
    index_file, adjacency_file, args...) };
}

template<typename Graph>
void RunBFS(Graph& graph,
  std::string_view graph_base,
  std::uint32_t start_vertex,
  bool print_result = false)
{
  auto breadth_first_search =
    famgraph::BreadthFirstSearch<Graph, MpiSubstrate>(graph);
  auto result = breadth_first_search(start_vertex);
  if (!print_result) return;
  auto max_distance = bfs_reference_output.at({ graph_base, start_vertex });
  REQUIRE(result.max_distance == max_distance);
}

template<typename Graph>
void RunKcore(Graph& graph,
  std::string_view graph_base,
  std::uint32_t kcore_k,
  bool print_result = false)
{
  auto kcore_decomposition = famgraph::KcoreDecomposition(graph);
  auto result = kcore_decomposition(kcore_k);
  if (!print_result) return;
  auto kth_core_size = kcore_reference_output.at({ graph_base, kcore_k });
  REQUIRE(result.kth_core_membership == kth_core_size);
}

template<typename Graph>
void RunConnectedComponents(Graph& graph,
  std::string_view graph_base,
  bool print_result = false)
{
  auto connected_components = famgraph::ConnectedComponents(graph);
  auto result = connected_components();
  if (!print_result) return;
  auto reference = connected_components_output.at(graph_base);
  REQUIRE(result.components == reference.total_components);
  REQUIRE(result.non_trivial_components == reference.non_trivial_components);
  REQUIRE(result.largest_component_size == reference.largest_component_size);
}

template<typename Graph>
void RunPageRank(Graph& graph,
  std::string_view graph_base,
  bool print_result = false)
{
  auto page_rank = famgraph::PageRank(graph);
  auto result = page_rank();
  if (!print_result) return;
  auto reference = pagerank_output.at(graph_base);

  REQUIRE(reference.top20.size() == result.topN.size());
  int const iteration_tolerance = 5;
  REQUIRE(
    std::abs(reference.iterations - result.iterations) <= iteration_tolerance);
  for (unsigned long i = 0; i < reference.top20.size(); ++i) {
    REQUIRE(reference.top20[i].first == result.topN[i].second);
    auto const percent_difference =
      std::abs((reference.top20[i].second - result.topN[i].first)
               / reference.top20[i].second);
    REQUIRE(percent_difference < reference.tolerance);
  }
}
}// namespace

void DoBFS(bool enable_print, bool enable_long_tests)
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  std::vector<BfsKey> inputs{ BfsKey{ small, 0 }, BfsKey{ gnutella, 0 } };
  if (enable_long_tests) inputs.push_back(BfsKey{ twitter7_symmetric, 1 });
  for (auto [graph_base, start_vertex] : inputs) {
    auto graph = CreateGraph(graph_base);
    RunBFS(graph, graph_base, start_vertex, enable_print);
  }
}

void DoCC(bool enable_print, bool enable_long_tests)
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  std::vector<std::string_view> inputs{ small_symmetric, gnutella_symmetric };
  if (enable_long_tests) inputs.push_back(twitter7_symmetric);
  for (auto graph_base : inputs) {
    auto graph = CreateGraph(graph_base);
    RunConnectedComponents(graph, graph_base, enable_print);
  }
}

int main(int argc, char *argv[])
{
  MPI_Init(&argc, &argv);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  DoBFS(rank == 0, false);
  DoCC(rank == 0, false);
  MPI_Finalize();
  return 0;
}