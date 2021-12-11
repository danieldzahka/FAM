#include "util.hpp"
#include <boost/align/align_up.hpp>
#include <sys/mman.h>

#include <spdlog/spdlog.h>

namespace {
constexpr auto PROT_RW = PROT_READ | PROT_WRITE;
constexpr auto MAP_ALLOC = MAP_PRIVATE | MAP_ANONYMOUS;
}

std::unique_ptr<void, std::function<void(void *)>>
  FAM::Util::mmap(std::uint64_t const size, bool const use_HP)
{
  spdlog::debug("RDMA mmap");
  auto constexpr HP = 21;
  auto const aligned_size = boost::alignment::align_up(size, HP);
  auto const alloc_size = use_HP ? aligned_size : size;
  auto const HP_FLAG = use_HP ? MAP_HUGETLB : 0;
  auto del = [alloc_size](void *p) noexcept
  {
    if (munmap(p, alloc_size)) spdlog::error("munmap() failed");
  };

  if (auto ptr = ::mmap(nullptr, aligned_size, PROT_RW, MAP_ALLOC | HP_FLAG, -1, 0)){
    return std::unique_ptr<void, std::function<void(void *)>>{ptr, del};
  }

  throw std::bad_alloc();
}
