#ifndef _FAM_H_
#define _FAM_H_

#include <string>
#include <memory>
#include <vector>

#include <FAM_segment.hpp>

namespace FAM {

namespace client {
  class FAM_control
  {
    class control_service_impl;
    class RDMA_service_impl;
    std::unique_ptr<control_service_impl> control_service;
    std::unique_ptr<RDMA_service_impl> RDMA_service;

  public:
    struct remote_region
    {
      std::uint64_t raddr;
      std::uint64_t length;
      std::uint32_t rkey;
    };

    struct local_region
    {
      void *laddr;
      std::uint64_t length;
      std::uint32_t lkey;
    };

    FAM_control(std::string const &control_addr,
      std::string const &RDMA_addr,
      std::string const &RDMA_port,
      int const rdma_channels);
    ~FAM_control();

    // Control services
    void ping();
    remote_region allocate_region(std::uint64_t size);
    remote_region mmap_remote_file(std::string const &filepath);

    // RDMA services
    local_region create_region(std::uint64_t const t_size,
      bool const use_HP,
      bool const write_allowed);

    // RDMA Dataplane
    void read(void *laddr,
      uint64_t raddr,
      uint32_t length,
      uint32_t lkey,
      uint32_t rkey,
      unsigned long channel) noexcept;

    void read(void *laddr,
      std::vector<FAM_segment> const &segs,
      uint32_t lkey,
      uint32_t rkey,
      unsigned long channel) noexcept;

    void write(void *laddr,
      uint64_t raddr,
      uint32_t length,
      uint32_t lkey,
      uint32_t rkey,
      unsigned long channel) noexcept;
  };
}// namespace client

namespace server {
  void run(std::string const &host, std::string const &port);
}// namespace server
}// namespace FAM

#endif
