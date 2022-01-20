#ifndef _FAMUTIL_H_
#define _FAMUTIL_H_

#include <memory>
#include <functional>
#include <string>

namespace FAM {
namespace Util {
  std::unique_ptr<void, std::function<void(void *)>>
    mmap(std::uint64_t const size, bool const use_HP);

  uint64_t file_size(std::string const &file);
  void copy_file(void *dest, std::string const &file, uint64_t const filesize);
}// namespace Util
}// namespace FAM

#endif//_FAMUTIL_H_
