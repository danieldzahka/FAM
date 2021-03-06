#include <functional>
#include <iostream>
#include <exception>

#include <spdlog/spdlog.h>
#include <boost/program_options.hpp>

#include <FAM.hpp>

int main(int argc, const char **argv)
{
  namespace po = boost::program_options;

  po::options_description desc{ "Options" };
  desc.add_options()("help,h", "Help screen")("server-addr,a",
    po::value<std::string>()->default_value("0.0.0.0"),
    "Server's IPoIB addr")(
    "port,p", po::value<std::string>()->default_value("50051"), "server port")(
    "memserver-port, m", po::value<std::uint64_t>()->default_value(35287));
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  if (!(vm.count("server-addr") && vm.count("port"))) {
    throw po::validation_error(
      po::validation_error::invalid_option_value, "Specify addr and port");
  }

  auto const host = vm["server-addr"].as<std::string>();
  auto const port = vm["port"].as<std::string>();
  auto const memserver_port = vm["memserver-port"].as<std::uint64_t>();

  spdlog::info("Starting Server");
  try {
    FAM::server::RunServer(host, port, memserver_port);
  } catch (std::exception const &e) {
    spdlog::error("Caught Runtime Exception {}", e.what());
  }
}
