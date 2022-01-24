#include <fgidx.hpp>

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
  throw std::runtime_error("CreateInstance() can't find file");
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
    throw std::runtime_error("CreateInstance(): can't open input stream");

  for (uint64_t i = 0; i < verts; ++i) {
    uint64_t a;
    input.read(reinterpret_cast<char *>(&a), sizeof(uint64_t));
    idx[i] = a;
    if (static_cast<unsigned long>(input.gcount()) != sizeof(uint64_t)) {
      throw std::runtime_error("can't Read index data");
    }
  }

  return fgidx::DenseIndex{ idx, boost::numeric_cast<uint32_t>(verts - 1) };
}

fgidx::DenseIndex::DenseIndex(uint64_t t_idx[], uint32_t const t_v_max)
  : idx{ t_idx }, v_max{ t_v_max }
{}
fgidx::DenseIndex::DenseIndex(std::unique_ptr<uint64_t[]> t_idx,
  uint32_t const t_v_max)
  : idx{ std::move(t_idx) }, v_max{ t_v_max }
{}

fgidx::DenseIndex::HalfInterval fgidx::DenseIndex::operator[](
  const uint32_t v) const noexcept
{
  auto const b = this->idx[v];
  auto const e = this->idx[v + 1];
  return fgidx::DenseIndex::HalfInterval{ b, e };
}
