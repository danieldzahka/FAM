#ifndef _FAM_H_
#define _FAM_H_

#include <string>
#include <memory>

namespace FAM {

namespace client {
  class FAM_control
  {
    class control_service_impl;
    class RDMA_service_impl;
    std::unique_ptr<control_service_impl> control_service;
    std::unique_ptr<RDMA_service_impl> RDMA_service;

  public:
    FAM_control(std::string const &control_addr,
      std::string const &RDMA_addr,
      std::string const &RDMA_port);
    ~FAM_control();

    // Control services
    void ping();
    void allocate_region(std::uint64_t size);
    void mmap_file();

    // RDMA services
    void create_connection();
    void *create_region(std::uint64_t const t_size,
      bool const use_HP,
      bool const write_allowed);

    // RDMA Dataplane
    void read(void *laddr, void *raddr, uint32_t length) noexcept;
    void write(void *laddr, void *raddr, uint32_t length) noexcept;
  };
}// namespace client

namespace server {
  void run(std::string const &host, std::string const &port);
}// namespace server
}// namespace FAM

#endif
