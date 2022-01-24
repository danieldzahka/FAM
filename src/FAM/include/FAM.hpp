#ifndef _FAM_H_
#define _FAM_H_

#include <string>
#include <memory>
#include <vector>

#include <FAM_segment.hpp>

namespace FAM {

namespace client {
  class FamControl
  {
    class ControlServiceImpl;
    class RdmaServiceImpl;
    std::unique_ptr<ControlServiceImpl> control_service;
    std::unique_ptr<RdmaServiceImpl> RDMA_service;

  public:
    struct RemoteRegion
    {
      std::uint64_t raddr;
      std::uint64_t length;
      std::uint32_t rkey;
    };

    struct LocalRegion
    {
      void *laddr;
      std::uint64_t length;
      std::uint32_t lkey;
    };

    FamControl(std::string const &control_addr,
      std::string const &RDMA_addr,
      std::string const &RDMA_port,
      int const rdma_channels);
    ~FamControl();

    // Control services
    void ping();
    RemoteRegion AllocateRegion(std::uint64_t size);
    RemoteRegion MmapRemoteFile(std::string const &filepath);

    // rdma services
    LocalRegion CreateRegion(std::uint64_t const t_size,
      bool const use_HP,
      bool const write_allowed);

    // rdma Dataplane
    void Read(void *laddr,
      uint64_t raddr,
      uint32_t length,
      uint32_t lkey,
      uint32_t rkey,
      unsigned long channel) noexcept;

    void Read(void *laddr,
      std::vector<FamSegment> const &segs,
      uint32_t lkey,
      uint32_t rkey,
      unsigned long channel) noexcept;

    void Write(void *laddr,
      uint64_t raddr,
      uint32_t length,
      uint32_t lkey,
      uint32_t rkey,
      unsigned long channel) noexcept;
  };
}// namespace client

namespace server {
  void Run(std::string const &host, std::string const &port);
}// namespace server
}// namespace FAM

#endif
