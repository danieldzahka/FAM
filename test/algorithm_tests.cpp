#include <catch2/catch.hpp>
#include <fmt/core.h>

#include <map>
#include <string_view>

#include <constants.hpp>
#include <famgraph.hpp>
#include <codec.hpp>
#include <famgraph_algorithms.hpp>

namespace {
using namespace std::literals::string_view_literals;

auto constexpr INPUTS_DIR = TEST_GRAPH_DIR;
auto constexpr LARGE_INPUTS_DIR = LARGE_TEST_GRAPH_DIR;
auto constexpr memserver_grpc_addr = MEMADDR;
auto constexpr ipoib_addr = MEMSERVER_IPOIB;
auto constexpr ipoib_port = MEMSERVER_RDMA_PORT;

static constexpr int threads = 10;

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

template<typename AdjacencyGraph = famgraph::LocalGraph<>, typename... Args>
AdjacencyGraph CreateGraph(std::string_view graph_base,
  std::string_view suffix,
  Args... args)
{
  auto index_file = fmt::format("{}.{}{}", graph_base, "idx", suffix);
  auto adjacency_file = fmt::format("{}.{}{}", graph_base, "adj", suffix);

  return { AdjacencyGraph::CreateInstance(
    index_file, adjacency_file, args...) };
}

template<typename Graph>
void RunBFS(Graph& graph,
  std::string_view graph_base,
  std::uint32_t start_vertex)
{
  auto breadth_first_search = famgraph::BreadthFirstSearch(graph);
  auto result = breadth_first_search(start_vertex);
  auto max_distance = bfs_reference_output.at({ graph_base, start_vertex });
  REQUIRE(result.max_distance == max_distance);
}

template<typename Graph>
void RunKcore(Graph& graph, std::string_view graph_base, std::uint32_t kcore_k)
{
  auto kcore_decomposition = famgraph::KcoreDecomposition(graph);
  auto result = kcore_decomposition(kcore_k);
  auto kth_core_size = kcore_reference_output.at({ graph_base, kcore_k });
  REQUIRE(result.kth_core_membership == kth_core_size);
}

template<typename Graph>
void RunConnectedComponents(Graph& graph, std::string_view graph_base)
{
  auto connected_components = famgraph::ConnectedComponents(graph);
  auto result = connected_components();
  auto reference = connected_components_output.at(graph_base);
  REQUIRE(result.components == reference.total_components);
  REQUIRE(result.non_trivial_components == reference.non_trivial_components);
  REQUIRE(result.largest_component_size == reference.largest_component_size);
}

template<typename Graph>
void RunPageRank(Graph& graph, std::string_view graph_base)
{
  auto page_rank = famgraph::PageRank(graph);
  auto result = page_rank();
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

std::vector<std::string_view> const vec{ "", "2" };
}// namespace

TEMPLATE_TEST_CASE_SIG("LocalGraph Breadth First Search",
  "[local]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  auto [graph_base, start_vertex] = GENERATE(
    BfsKey{ small, 0 }, BfsKey{ gnutella, 0 }, BfsKey{ last_vert_nonempty, 0 });
  auto graph = CreateGraph<famgraph::LocalGraph<T>>(graph_base, vec[V]);
  RunBFS(graph, graph_base, start_vertex);
}

TEMPLATE_TEST_CASE_SIG("RemoteGraph Breadth First Search",
  "[rdma]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  auto [graph_base, start_vertex] = GENERATE(
    BfsKey{ small, 0 }, BfsKey{ gnutella, 0 }, BfsKey{ last_vert_nonempty, 0 });
  int const rdma_channels = 5;
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, rdma_channels);
  auto graph = CreateGraph<famgraph::RemoteGraph<T>>(graph_base,
    vec[V],
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
  RunBFS(graph, graph_base, start_vertex);
}

TEMPLATE_TEST_CASE_SIG("LocalGraph Kcore Decomposition",
  "[local]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  auto [graph_base, k] = GENERATE(KcoreKey{ small_symmetric, 2 },
    KcoreKey{ gnutella_symmetric, 5 },
    KcoreKey{ gnutella_symmetric, 6 },
    KcoreKey{ gnutella_symmetric, 7 });

  auto graph = CreateGraph<famgraph::LocalGraph<T>>(graph_base, vec[V]);
  RunKcore(graph, graph_base, k);
}

TEMPLATE_TEST_CASE_SIG("RemoteGraph Kcore Decomposition",
  "[rdma]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  auto [graph_base, k] = GENERATE(KcoreKey{ small_symmetric, 2 },
    KcoreKey{ gnutella_symmetric, 5 },
    KcoreKey{ gnutella_symmetric, 6 },
    KcoreKey{ gnutella_symmetric, 7 });

  int const rdma_channels = 5;
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, rdma_channels);
  auto graph = CreateGraph<famgraph::RemoteGraph<T>>(graph_base,
    vec[V],
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
  RunKcore(graph, graph_base, k);
}

TEMPLATE_TEST_CASE_SIG("LocalGraph ConnectedComponents",
  "[local]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  auto graph_base = GENERATE(small_symmetric, gnutella_symmetric);
  auto graph = CreateGraph<famgraph::LocalGraph<T>>(graph_base, vec[V]);
  RunConnectedComponents(graph, graph_base);
}

TEMPLATE_TEST_CASE_SIG("RemoteGraph ConnectedComponents",
  "[rdma]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  auto graph_base = GENERATE(small_symmetric, gnutella_symmetric);
  int const rdma_channels = 5;
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, rdma_channels);
  auto graph = CreateGraph<famgraph::RemoteGraph<T>>(graph_base,
    vec[V],
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
  RunConnectedComponents(graph, graph_base);
}

TEMPLATE_TEST_CASE_SIG("LocalGraph PageRank",
  "[local]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  auto graph_base = GENERATE(gnutella);
  auto graph = CreateGraph<famgraph::LocalGraph<T>>(graph_base, vec[V]);
  RunPageRank(graph, graph_base);
}

TEMPLATE_TEST_CASE_SIG("RemoteGraph PageRank",
  "[rdma]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  auto graph_base = GENERATE(gnutella);
  int const rdma_channels = 5;
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, rdma_channels);
  auto graph = CreateGraph<famgraph::RemoteGraph<T>>(graph_base,
    vec[V],
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
  RunPageRank(graph, graph_base);
}

TEMPLATE_TEST_CASE_SIG("Large Graph LocalGraph Breadth First Search",
  "[local][long]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  auto [graph_base, start_vertex] = GENERATE(BfsKey{ twitter7_symmetric, 1 });
  auto graph = CreateGraph<famgraph::LocalGraph<T>>(graph_base, vec[V]);
  RunBFS(graph, graph_base, start_vertex);
}

TEMPLATE_TEST_CASE_SIG("Large Graph RemoteGraph Breadth First Search",
  "[rdma][long]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  auto [graph_base, start_vertex] = GENERATE(BfsKey{ twitter7_symmetric, 1 });
  int const rdma_channels = 10;
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, rdma_channels);
  auto graph = CreateGraph<famgraph::RemoteGraph<T>>(graph_base,
    vec[V],
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
  RunBFS(graph, graph_base, start_vertex);
}

TEMPLATE_TEST_CASE_SIG("Large Graph LocalGraph Kcore Decomposition",
  "[local][long]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  auto [graph_base, k] = GENERATE(KcoreKey{ twitter7_symmetric, 100 });
  auto graph = CreateGraph<famgraph::LocalGraph<T>>(graph_base, vec[V]);
  RunKcore(graph, graph_base, k);
}

TEMPLATE_TEST_CASE_SIG("Large Graph RemoteGraph Kcore Decomposition",
  "[rdma][long]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  auto [graph_base, k] = GENERATE(KcoreKey{ twitter7_symmetric, 100 });
  int const rdma_channels = 10;
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, rdma_channels);
  auto graph = CreateGraph<famgraph::RemoteGraph<T>>(graph_base,
    vec[V],
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
  RunKcore(graph, graph_base, k);
}

TEMPLATE_TEST_CASE_SIG("Large Graph LocalGraph ConnectedComponents",
  "[local][long]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  auto graph_base = GENERATE(twitter7_symmetric);
  auto graph = CreateGraph<famgraph::LocalGraph<T>>(graph_base, vec[V]);
  RunConnectedComponents(graph, graph_base);
}

TEMPLATE_TEST_CASE_SIG("Large Graph RemoteGraph ConnectedComponents",
  "[rdma][long]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  auto graph_base = GENERATE(twitter7_symmetric);
  int const rdma_channels = 10;
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, rdma_channels);
  auto graph = CreateGraph<famgraph::RemoteGraph<T>>(graph_base,
    vec[V],
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
  RunConnectedComponents(graph, graph_base);
}

TEMPLATE_TEST_CASE_SIG("Large Graph LocalGraph PageRank",
  "[local][long]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  auto graph_base = GENERATE(twitter7);
  auto graph = CreateGraph<famgraph::LocalGraph<T>>(graph_base, vec[V]);
  RunPageRank(graph, graph_base);
}

TEMPLATE_TEST_CASE_SIG("Large Graph RemoteGraph PageRank",
  "[rdma][long]",
  ((typename T, int V), T, V),
  (NopDecompressor, 0),
  (famgraph::tools::DeltaDecompressor, 1))
{
  auto graph_base = GENERATE(twitter7);
  int const rdma_channels = 10;
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, rdma_channels);
  auto graph = CreateGraph<famgraph::RemoteGraph<T>>(graph_base,
    vec[V],
    memserver_grpc_addr,
    ipoib_addr,
    ipoib_port,
    rdma_channels);
  RunPageRank(graph, graph_base);
}