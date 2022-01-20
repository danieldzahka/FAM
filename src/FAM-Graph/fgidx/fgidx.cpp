#include <fgidx.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

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
  throw std::runtime_error("make_dense_idx() can't find file");
}
}

fgidx::dense_idx fgidx::dense_idx::make_dense_idx(std::string const &filepath,
  uint64_t const n_edges)
{
  auto const verts = get_num_verts(filepath);
  auto idx = new uint64_t[verts + 1];
  idx[verts] = n_edges;

  std::ifstream input(filepath, std::ios::binary);
  if (!input)
    throw std::runtime_error("make_dense_idx(): can't open input stream");

  for (uint64_t i = 0; i < verts; ++i) {
    uint64_t a;
    input.read(reinterpret_cast<char *>(&a), sizeof(uint64_t));
    idx[i] = a;
    if (static_cast<unsigned long>(input.gcount()) != sizeof(uint64_t)) {
      throw std::runtime_error("can't read index data");
    }
  }
  
  return fgidx::dense_idx{ idx };
}

fgidx::dense_idx::dense_idx(uint64_t t_idx[]) : idx{ t_idx } {}
fgidx::dense_idx::dense_idx(std::unique_ptr<uint64_t[]> t_idx)
  : idx{ std::move(t_idx) }
{}

fgidx::dense_idx::half_interval fgidx::dense_idx::operator[](
  const uint32_t v) const noexcept
{
  auto const b = this->idx[v];
  auto const e = this->idx[v + 1];
  return fgidx::dense_idx::half_interval{ b, e };
}
