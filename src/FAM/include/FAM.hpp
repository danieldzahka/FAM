#ifndef _FAM_H_
#define _FAM_H_

#include <string>
#include <memory>
#include <vector>

#include <FAM_segment.hpp>

namespace FAM {
namespace server {
  void RunServer(std::string const &host, std::string const &port, std::uint64_t const &memserver_port);
}// namespace server
class FamControl
{
  class ControlServiceImpl;
  class RdmaServiceImpl;
  std::unique_ptr<ControlServiceImpl> control_service_;
  std::unique_ptr<RdmaServiceImpl> rdma_service_;

public:
  int const rdma_channels_;
  
  struct RemoteRegion
  {
    uint64_t raddr;
    uint64_t length;
    uint32_t rkey;
  };

  struct LocalRegion
  {
    void *laddr;
    uint64_t length;
    uint32_t lkey;
  };

  FamControl(std::string const &control_addr,
    std::string const &ipoib_addr,
    std::string const &ipoib_port,
    int const rdma_channels);
  ~FamControl();

  // Control services
  void Ping();
  RemoteRegion AllocateRegion(uint64_t size);
  RemoteRegion MmapRemoteFile(std::string const &filepath);

  // rdma services
  LocalRegion CreateRegion(uint64_t const t_size,
    bool const use_hugepages,
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
}// namespace FAM

#endif
