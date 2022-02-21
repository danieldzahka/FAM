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

    frontier->Set(start_vertex);
    while (!frontier->IsEmpty()) {

      frontier->Clear();
      std::swap(frontier, next_frontier);
    }

    return Result{};
  }
};
}// namespace famgraph
#endif// FAM_FAMGRAPH_ALGORITHMS_HPP
