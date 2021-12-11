#ifndef _FAMUTIL_H_
#define _FAMUTIL_H_

#include <memory>
#include <functional>

namespace FAM {
namespace Util {
  std::unique_ptr<void, std::function<void(void *)>>
    mmap(std::uint64_t const size, bool const use_HP);
}
}// namespace FAM

#endif//_FAMUTIL_H_
