#ifndef FAM_NETWORK_SUBSTRATE_HPP
#define FAM_NETWORK_SUBSTRATE_HPP

#include <famgraph.hpp>
#include <mpi.h>
#include <string>//remove later
#include <iostream>//remove later

class MpiSubstrate
{
public:
  static void Log(std::string const& message)
  {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cerr << rank << ": " << message << std::endl;
  }

  static bool IsEmpty(famgraph::VertexSubset& frontier) noexcept
  {
    return frontier.IsEmpty2();
  }

  static void SyncFrontier(famgraph::VertexSubset& frontier) noexcept
  {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    auto *buffer = frontier.GetTable();
    auto const count = frontier.Count();
    MPI_Allreduce(MPI_IN_PLACE,
      buffer,
      static_cast<int>(count),
      MPI_UINT64_T,
      MPI_BOR,
      MPI_COMM_WORLD);
  }

  template<typename Graph> static void SyncVertexTable(Graph& graph) noexcept
  {
    auto const num_vertices = graph.NumVertices();
    auto *array = graph.VertexArray();
    MPI_Allreduce(
      MPI_IN_PLACE, array, num_vertices, MPI_UINT32_T, MPI_MIN, MPI_COMM_WORLD);
  }

  template<typename Graph>
  static tbb::blocked_range<famgraph::VertexLabel> AuthoritativeRange(
    Graph const& graph) noexcept
  {
    int mpi_rank;
    int mpi_procs;

    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_procs);

    auto const rank = static_cast<uint32_t>(mpi_rank);
    auto const procs = static_cast<uint32_t>(mpi_procs);

    auto const vertices = graph.NumVertices();
    auto const w = vertices / procs;
    auto const l = rank * w;
    auto const r = rank == procs ? vertices : (rank + 1) * w;
    return { l, r };
  }
};

#endif// FAM_NETWORK_SUBSTRATE_HPP
