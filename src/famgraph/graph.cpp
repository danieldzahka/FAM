#include <famgraph.hpp>

famgraph::RemoteGraph famgraph::RemoteGraph::CreateInstance(
  const std::string &path)
{
  // ping memory server to mmap the remote file and get num edges
  // also can create local buffers for us.
  uint64_t const edges = 10;

  return RemoteGraph{ fgidx::dense_idx::CreateInstance(path, edges) };
}
famgraph::RemoteGraph::RemoteGraph(fgidx::dense_idx &&t_idx)
  : idx_{ std::move(t_idx) }
{}