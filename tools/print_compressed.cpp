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

    auto const I = ToVector<uint64_t>(idxstream, verts);
    auto const Aj = ToVector<uint32_t>(adjstream, edges);

    for (int i = 0; i < verts; ++i) {
      //      std::cout << i << " " << I[i] << "\n";
      auto const A = I[i];
      auto const B = i == verts - 1 ? edges : I[i + 1];
      auto const l = B - A;
      if (l == 0) continue;
      auto const buf = Aj.data() + A;
      famgraph::tools::DeltaDecompressor::Decompress(
        buf, l, [&](auto d, auto u) { std::cout << i << " " << d << "\n"; });
    }

    return 0;
  } catch (std::exception const& ex) {
    std::cout << "Caught Runtime Exception: " << ex.what() << std::endl;
    return 1;
  }
}