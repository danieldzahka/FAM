#include <functional>
#include <iostream>

#include <spdlog/spdlog.h>
#include <docopt/docopt.h>

static constexpr auto USAGE =
  R"(main.

    Usage:
          main
          main (-h | --help)
          main --version
 Options:
          -h --help     Show this screen.
          --version     Show version.
)";

int main(int argc, const char **argv)
{
  std::map<std::string, docopt::value> args = docopt::docopt(USAGE,
    { std::next(argv), std::next(argv, argc) },
    true,// show help if requested
    "main 2.0");// version string

  //Use the default logger (stdout, multi-threaded, colored)
  spdlog::info("Hello, {}!", "World");

  fmt::print("Hello, from {}\n", "{fmt}");
}
