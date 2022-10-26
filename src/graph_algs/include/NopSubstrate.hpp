#ifndef FAM_NOPSUBSTRATE_HPP
#define FAM_NOPSUBSTRATE_HPP

#include <famgraph.hpp>

class NopSubstrate
{
public:
  static bool IsEmpty(famgraph::VertexSubset& frontier) noexcept
  {
    return frontier.IsEmpty();
  }

  template<typename... Args> constexpr static void Log(Args&...) noexcept {}

  template<typename... Args>
  constexpr static void SyncFrontier(Args&...) noexcept
  {}

  template<typename... Args>
  constexpr static void SyncVertexTable(Args&...) noexcept
  {}

  template<typename Graph>
  static tbb::blocked_range<famgraph::VertexLabel> AuthoritativeRange(
    Graph const& graph) noexcept
  {
    auto const num_vertices = graph.NumVertices();
    return { 0, num_vertices };
  }
};
#endif// FAM_NOPSUBSTRATE_HPP
