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
  using VertexLabel = std::uint32_t;
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
}// namespace famgraph
#endif// FAM_FAMGRAPH_ALGORITHMS_HPP
