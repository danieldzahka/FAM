#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <codec.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace {

template<typename T> auto num_elements(fs::path const& p)
{
  auto const file_size = fs::file_size(p);
  auto const n = file_size / sizeof(T);
  return n;
}

template<typename T> auto read_one(std::ifstream& s)
{
  T x;
  s.read(reinterpret_cast<char *>(&x), sizeof(T));
  if (static_cast<unsigned long>(s.gcount()) != sizeof(T)) {
    throw std::runtime_error("can't read index data");
  }
  return x;
}

template<typename T> std::vector<T> ToVector(std::ifstream& s, uint32_t n)
{
  std::vector<T> ret;
  for (unsigned int i = 0; i < n; ++i) {
    auto x = read_one<T>(s);
    ret.push_back(x);
  }

  return ret;
}

template<typename T> void write_one(fs::ofstream& o, T const x)
{
  o.write(reinterpret_cast<char const *>(&x), sizeof(x));
}

void CompressGraph(std::vector<uint64_t> const& idx,
  fs::ofstream& idx_os,
  fs::ofstream& adj_os,
  uint64_t edges,
  std::ifstream& adj_is)
{
  auto const n = idx.size();
  uint64_t off = 0;
  for (uint32_t i = 0; i < n; ++i) {
    write_one(idx_os, off);
    auto const a = idx[i];
    auto const b = i == n - 1 ? edges : idx[i + 1];
    auto const degree = b - a;
    if (degree == 0) continue;
    auto uncompressed = ToVector<uint32_t>(adj_is, degree);
    famgraph::tools::CompressionOptions options{ 10, 1000 };
    auto const [compressed, count] =
      famgraph::tools::Compress(uncompressed.data(), degree, options);
    for (int j = 0; j < count; ++j) { write_one(adj_os, compressed[j]); }
    off += count;
  }
}


void validate_file(fs::path const& p)
{
  if (!(fs::exists(p) && fs::is_regular_file(p))) {
    throw std::runtime_error("file not found");
  }
}

}// namespace

int main(int argc, char *argv[])
{
  try {
    po::options_description desc{ "Options" };
    desc.add_options()("help,h", "Help screen")("idx,i",
      po::value<std::string>(),
      ".idx filepath")("adj,a", po::value<std::string>(), ".adj filepath");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    };

    std::string file = vm["idx"].as<std::string>();
    fs::path index(file);
    std::string file2 = vm["adj"].as<std::string>();
    fs::path adj(file2);

    BOOST_LOG_TRIVIAL(info) << ".idx file " << index;
    validate_file(index);
    BOOST_LOG_TRIVIAL(info) << ".adj file " << adj;
    validate_file(adj);

    auto const verts = num_elements<uint64_t>(index);
    auto const edges = num_elements<uint32_t>(adj);

    std::ifstream idxstream(index.c_str(), std::ios::binary);
    if (!idxstream) throw std::runtime_error("Couldn't open file!");

    std::ifstream adjstream(adj.c_str(), std::ios::binary);
    if (!adjstream) throw std::runtime_error("Couldn't open file!");

    auto index_out = index.replace_extension(".idx2");
    auto adj_out = adj.replace_extension(".adj2");
    fs::ofstream index_os(index_out);
    fs::ofstream adj_os(adj_out);

    auto const I = ToVector<uint64_t>(idxstream, verts);
    CompressGraph(I, index_os, adj_os, edges, adjstream);

    index_os.close();
    adj_os.close();
    return 0;
  } catch (std::exception const& ex) {
    std::cout << "Caught Runtime Exception: " << ex.what() << std::endl;
    return 1;
  }
}