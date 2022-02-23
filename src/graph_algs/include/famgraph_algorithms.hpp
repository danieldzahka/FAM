//
// Created by daniel on 2/19/22.
//

#ifndef FAM_FAMGRAPH_ALGORITHMS_HPP
#define FAM_FAMGRAPH_ALGORITHMS_HPP

#include <limits>
#include <atomic>
#include <famgraph.hpp>

namespace famgraph {
template<typename AdjacencyGraph> class BreadthFirstSearch
{
  constexpr static auto NULL_VERT = std::numeric_limits<VertexLabel>::max();

  struct Vertex
  {
    std::atomic<VertexLabel> parent{ NULL_VERT };
  };

  famgraph::Graph<Vertex, AdjacencyGraph> graph_;

public:
  BreadthFirstSearch(AdjacencyGraph &graph) : graph_(graph) {}

  struct Result
  {
    std::uint32_t max_distance;
  };

  Result operator()(VertexLabel start_vertex)
  {
    auto const max_v = this->graph_.max_v();
    famgraph::VertexSubset frontierA{ max_v };
    famgraph::VertexSubset frontierB{ max_v };

    auto *frontier = &frontierA;
    auto *next_frontier = &frontierB;

    auto &graph = this->graph_;
    auto &adj_graph = graph.getAdjacencyGraph();

    auto push = [&](uint32_t const v,
                  uint32_t const w,
                  uint64_t const /*v_degree*/) noexcept {
      auto expect = NULL_VERT;
      const auto w_was_updated = graph[w].parent.compare_exchange_strong(
        expect, v, std::memory_order_relaxed, std::memory_order_relaxed);
      if (w_was_updated) next_frontier->Set(w);
    };

    std::uint32_t rounds = 0;
    frontier->Set(start_vertex);
    while (!frontier->IsEmpty()) {
      EdgeMap(adj_graph, *frontier, push);
      frontier->Clear();
      std::swap(frontier, next_frontier);
      ++rounds;
    }

    return Result{ rounds > 0 ? rounds - 1 : 0 };
  }
};

template<typename AdjacencyGraph> class KcoreDecomposition
{
  using VertexDegreeClass = std::uint32_t;
  struct Vertex
  {
    std::atomic<VertexDegreeClass> degree;
  };

  famgraph::Graph<Vertex, AdjacencyGraph> graph_;

  VertexLabel KthCoreSize(VertexDegreeClass k) noexcept
  {
    VertexLabel size = 0;
    for (VertexLabel v = 0; v <= this->graph_.max_v(); ++v) {
      if (this->graph_[v].degree >= k) ++size;
    }
    return size;
  }

public:
  KcoreDecomposition(AdjacencyGraph &graph) : graph_(graph) {}

  struct Result
  {
    std::uint32_t kth_core_membership;
  };

  Result operator()(VertexLabel k)
  {
    auto const max_v = this->graph_.max_v();
    famgraph::VertexSubset frontierA{ max_v };
    famgraph::VertexSubset frontierB{ max_v };

    auto *frontier = &frontierA;
    auto *next_frontier = &frontierB;

    auto &graph = this->graph_;
    auto &adj_graph = graph.getAdjacencyGraph();

    famgraph::VertexMap(graph, [&](Vertex &vertex, VertexLabel v) noexcept {
      auto const d = static_cast<std::uint32_t>(adj_graph.Degree(v));
      vertex.degree.store(d, std::memory_order_relaxed);
      if (d < k) frontier->Set(v);
    });

    auto push = [&](uint32_t const v,
                  uint32_t const w,
                  uint64_t const /*v_degree*/) noexcept {
      auto old = graph[v].degree.fetch_sub(1, std::memory_order_relaxed);
      if (old == k) next_frontier->Set(w);
    };

    while (!frontier->IsEmpty()) {
      PrintVertexSubset(*frontier);
      EdgeMap(adj_graph, *frontier, push);
      frontier->Clear();
      std::swap(frontier, next_frontier);
    }

    return Result{ KthCoreSize(k) };
  }
};


}// namespace famgraph
#endif// FAM_FAMGRAPH_ALGORITHMS_HPP
