#include <famgraph.hpp>
#include "FAM.hpp"
#include <fmt/core.h>//TODO: Delete this dep

famgraph::VertexSubset::VertexSubset(uint32_t max_v)
  : bitmap_(new std::uint64_t[Offset(max_v) + 1]), max_v_(max_v)
{
  this->Clear();
}
uint32_t famgraph::VertexSubset::GetMaxV() const noexcept { return max_v_; }
std::uint64_t *famgraph::VertexSubset::GetTable() noexcept
{
  return this->bitmap_.get();
}
std::uint32_t famgraph::VertexSubset::Count() noexcept
{
  return Offset(this->max_v_) + 1;
}

void famgraph::PrintVertexSubset(
  famgraph::VertexSubset const& vertex_subset) noexcept
{
  auto const max_v = vertex_subset.GetMaxV();
  fmt::print("(");
  for (VertexLabel i = 0; i <= max_v; ++i) {
    if (vertex_subset[i]) fmt::print("{}, ", i);
  }
  fmt::print(")\n");
}
