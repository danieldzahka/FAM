#include <iostream>
#include <stdexcept>
#include <vector>
#include <utility>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <spdlog/spdlog.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <oneapi/dpl/execution>
#include <oneapi/dpl/algorithm>
#pragma GCC diagnostic pop

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using std::vector;

namespace {
struct csr_graph
{
  vector<uint64_t> index;
  vector<uint32_t> dest;

  csr_graph(vector<uint64_t> i, vector<uint32_t> d)
    : index(std::move(i)), dest(std::move(d))
  {}
};

}// namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

struct csr_graph edge_list_to_csr(
  vector<std::pair<uint32_t, uint32_t>> const& v,
  uint32_t const max_vert)
{
  vector<uint64_t> idx(max_vert + 1, v.size() - 1);
  vector<uint32_t> dest(v.size(), 0);

  uint32_t prev_vertex_from = 0;
  uint64_t count = 0;
  idx[0] = 0;
  uint32_t vertex_from = 0;
  for (auto const& p : v) {
    vertex_from = p.first;
    uint32_t vertex_to = p.second;
    if (vertex_from != prev_vertex_from) {
      // Fill in all rows that have no out edges
      for (uint32_t row = prev_vertex_from + 1; row <= vertex_from; ++row) {
        idx[row] = count;
      }
      prev_vertex_from = vertex_from;
    }
    dest[count] = vertex_to;
    count++;
  }
  for (uint32_t s = vertex_from + 1; s <= max_vert; ++s) { idx[s] = v.size(); }
  return csr_graph(std::move(idx), std::move(dest));
}

void encode_unweighted(fs::path const& p,
  fs::path const& index,
  fs::path const& adj,
  po::variables_map const& vm)
{
  bool make_undirected = vm.count("make-undirected") != 0;
  fs::ifstream ifs(p);
  uint32_t a, b;
  std::vector<std::pair<uint32_t, uint32_t>> v;
  uint32_t max_vert = 0;
  while (ifs >> a >> b) {
    v.push_back(std::make_pair(a, b));
    if (make_undirected) { v.push_back(std::make_pair(b, a)); }
    max_vert = std::max(max_vert, std::max(a, b));
  }

  //  BOOST_LOG_TRIVIAL(info) << "|V| " << max_vert;
  //  BOOST_LOG_TRIVIAL(info) << "|E| " << v.size();

  // sort the vector by starting vertex
  if (!vm.count("sorted")) {
    oneapi::dpl::sort(oneapi::dpl::execution::par_unseq, v.begin(), v.end());
  }

  csr_graph g = edge_list_to_csr(v, max_vert);

  //  BOOST_LOG_TRIVIAL(info) << "Writing " << g.index.size() << " vertices";
  //  BOOST_LOG_TRIVIAL(info) << "Writing " << g.dest.size() << " edges";
  uint32_t v_written = 0;
  uint64_t e_written = 0;

  fs::ofstream index_ostream(index);
  fs::ofstream adj_ostream(adj);
  for (auto i : g.index) {
    index_ostream.write((char *)&i, sizeof(i));
    v_written++;
  }
  for (auto i : g.dest) {
    adj_ostream.write((char *)&i, sizeof(i));
    e_written++;
  }

  //  BOOST_LOG_TRIVIAL(info) << "Wrote " << v_written << " vertices";
  //  BOOST_LOG_TRIVIAL(info) << "Wrote " << e_written << " edges";

  index_ostream.close();
  adj_ostream.close();
}

int main(int argc, char *argv[])
{
  try {
    po::options_description desc{ "Options" };
    desc.add_options()("help,h", "Help screen")("infile,i",
      po::value<std::string>(),
      "input filepath")("outdir,o", po::value<std::string>(), "ouput dir")(
      "sorted,s", "set if input edgelist is sorted by origin vertex")(
      "make-undirected", "add a reverse edge for each edge in the list");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) std::cout << desc << std::endl;

    std::string file = vm["infile"].as<std::string>();
    std::string outdir = vm["outdir"].as<std::string>();
    std::string outdir2 = vm["outdir"].as<std::string>();
    fs::path p(file);
    fs::path p2(outdir.append(p.stem().c_str()));
    fs::path p3(outdir2.append(p.stem().c_str()));
    fs::path index(p2.replace_extension(".idx"));
    fs::path adj(p3.replace_extension(".adj"));

    spdlog::info("Writing index file: {}\nWriting adj file: {}",
      index.c_str(),
      adj.c_str());

    if (fs::exists(p) && fs::is_regular_file(p)) {
      encode_unweighted(p, index, adj, vm);
    } else {
      throw std::runtime_error("Input file not found");
    }
    return 0;
  } catch (std::exception const& ex) {
    std::cout << "Caught Runtime Exception: " << ex.what() << std::endl;
    return 1;
  }
}

#pragma GCC diagnostic pop
