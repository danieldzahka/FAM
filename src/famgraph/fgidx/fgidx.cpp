#include <fgidx.hpp>
#include <fmt/core.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <boost/numeric/conversion/cast.hpp>

namespace {
auto get_num_verts(std::string const &file)
{
  namespace fs = boost::filesystem;
  fs::path p(file);
  if (fs::exists(p) && fs::is_regular_file(p)) {
    const uint64_t file_size = fs::file_size(p);
    const uint64_t num_vertices = file_size / sizeof(uint64_t);
    return num_vertices;
  }
  throw std::runtime_error(
    fmt::format("CreateInstance() can't find file: {}", file));
}
}// namespace

fgidx::DenseIndex fgidx::DenseIndex::CreateInstance(std::string const &filepath,
  uint64_t n_edges)
{
  auto const verts = get_num_verts(filepath);
  auto idx = new uint64_t[verts + 1];
  idx[verts] = n_edges;

  std::ifstream input(filepath, std::ios::binary);
  if (!input)
    throw std::runtime_error(fmt::format(
      "CreateInstance(): can't open input stream file: {}", filepath));

  for (uint64_t i = 0; i < verts; ++i) {
    uint64_t a;
    input.read(reinterpret_cast<char *>(&a), sizeof(uint64_t));
    idx[i] = a;
    if (static_cast<unsigned long>(input.gcount()) != sizeof(uint64_t)) {
      throw std::runtime_error("can't Read index data");
    }

  }

  uint64_t max_out_degree = 0;
  for (uint64_t i = 0; i < verts; ++i){
    max_out_degree = std::max(max_out_degree, idx[i + 1] - idx[i]);
  }
  return fgidx::DenseIndex{
    idx, boost::numeric_cast<uint32_t>(verts - 1), max_out_degree
  };
}

fgidx::DenseIndex::DenseIndex(uint64_t t_idx[],
  uint32_t const t_v_max,
  uint64_t const t_max_out_degree)
  : idx{ t_idx }, v_max{ t_v_max }, max_out_degree{ t_max_out_degree }
{}
fgidx::DenseIndex::DenseIndex(std::unique_ptr<uint64_t[]> t_idx,
  uint32_t t_v_max,
  uint64_t t_max_out_degree)
  : idx{ std::move(t_idx) }, v_max{ t_v_max }, max_out_degree{
      t_max_out_degree
    }
{}

fgidx::DenseIndex::HalfInterval fgidx::DenseIndex::operator[](
  const uint32_t v) const noexcept
{
  auto const b = this->idx[v];
  auto const e = this->idx[v + 1];
  return fgidx::DenseIndex::HalfInterval{ b, e };
}
fgidx::AdjacencyArray fgidx::CreateAdjacencyArray(const std::string &filepath)
{
  namespace fs = boost::filesystem;
  fs::path p(filepath);
  if (!(fs::exists(p) && fs::is_regular_file(p)))
    throw std::runtime_error(
      fmt::format("CreateAdjacencyArray() can't find file: {}", filepath));

  const uint64_t file_size = fs::file_size(p);
  const uint64_t num_edges = file_size / sizeof(uint32_t);

  auto *idx = new uint32_t[num_edges];

  std::ifstream input(filepath, std::ios::binary);
  if (!input)
    throw std::runtime_error(fmt::format(
      "CreateAdjacencyArray(): can't open input stream file: {}", filepath));

  for (uint64_t i = 0; i < num_edges; ++i) {
    uint32_t a;
    input.read(reinterpret_cast<char *>(&a), sizeof(uint32_t));
    idx[i] = a;
    if (static_cast<unsigned long>(input.gcount()) != sizeof(uint32_t)) {
      throw std::runtime_error(
        fmt::format("can't Read adj data file: {}", filepath));
    }
  }

  return { num_edges, std::unique_ptr<uint32_t[]>(idx) };
}
