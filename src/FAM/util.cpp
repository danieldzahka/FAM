#include "util.hpp"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <spdlog/spdlog.h>

#include <boost/filesystem.hpp>//REMOVE

namespace {
constexpr auto PROT_RW = PROT_READ | PROT_WRITE;
constexpr auto MAP_ALLOC = MAP_PRIVATE | MAP_ANONYMOUS;

template<class T>
auto constexpr align_up(T value, std::size_t alignment) noexcept
{
  return T((value + (T(alignment) - 1)) & ~T(alignment - 1));
}

class file_mapper
{
  constexpr static uint64_t default_chunk = 10UL * (1 << 30);// 30 GB
  uint64_t const chunk_size{ default_chunk };
  uint64_t offset{ 0 };
  uint64_t filesize;
  int fd;

public:
  file_mapper(std::string const &file, uint64_t t_fsize)
    : filesize{ t_fsize }, fd{ open(file.c_str(), O_RDONLY) }
  {
    if (this->fd == -1) {
      throw std::runtime_error("open() failed on .adj file");
    }
  }

  ~file_mapper() { close(this->fd); }

  file_mapper(const file_mapper &) = delete;
  file_mapper &operator=(const file_mapper &) = delete;

  bool has_next() noexcept { return this->offset < this->filesize; }

  auto operator()()
  {
    auto length = std::min(this->chunk_size, this->filesize - this->offset);
    auto del = [length](void *p) {
      auto r = munmap(p, length);
      if (r) throw std::runtime_error("munmap chunk failed");
    };

    auto constexpr flags = MAP_PRIVATE | MAP_POPULATE;
    auto ptr = mmap(
      0, length, PROT_READ, flags, this->fd, static_cast<long>(this->offset));
    this->offset += length;

    if (ptr == MAP_FAILED) throw std::runtime_error("mmap file chunk failed");

    return make_pair(std::unique_ptr<void, decltype(del)>(ptr, del), length);
  }
};
}// namespace

std::unique_ptr<void, std::function<void(void *)>>
  FAM::Util::mmap(std::uint64_t const size, bool const use_HP)
{
  spdlog::debug("rdma mmap");
  std::size_t constexpr HP = 1 << 21;
  auto const aligned_size = align_up(size, HP);
  auto const alloc_size = use_HP ? aligned_size : size;
  auto const HP_FLAG = use_HP ? MAP_HUGETLB : 0;
  auto del = [alloc_size](void *p) noexcept {
    if (munmap(p, alloc_size)) spdlog::error("munmap() failed");
  };

  auto ptr = ::mmap(nullptr, aligned_size, PROT_RW, MAP_ALLOC | HP_FLAG, -1, 0);

  if (ptr == MAP_FAILED) throw std::bad_alloc();

  return std::unique_ptr<void, std::function<void(void *)>>{ ptr, del };
}

uint64_t FAM::Util::file_size(std::string const &file)
{
  namespace fs = boost::filesystem;

  fs::path p(file);
  if (!(fs::exists(p) && fs::is_regular_file(p)))
    throw std::runtime_error("mmap RPC: requested file not found");

  return fs::file_size(p);
}

void FAM::Util::copy_file(void *dest,
  std::string const &file,
  uint64_t const filesize)
{
  auto array = reinterpret_cast<char *>(dest);
  file_mapper get_mapped_chunk{ file, filesize };

  while (get_mapped_chunk.has_next()) {
    auto const [fptr, len] = get_mapped_chunk();
    std::memcpy(array, fptr.get(), len);
    array += len;
  }
}
