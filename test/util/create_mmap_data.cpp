#include <fstream>
#include <string>

#include <constants.hpp>

namespace {
auto const mmap_test2 = MMAP_TEST2;
}// namespace

int main()
{
  std::fstream file(
    mmap_test2, std::ios::out | std::ios::trunc | std::ios::binary);
  auto const N = 20000;
  for (int i = 0; i < N; ++i) {
    file.write(reinterpret_cast<char *>(&i), sizeof(i));
  }
  return 0;
}