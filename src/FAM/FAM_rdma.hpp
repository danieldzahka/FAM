#ifndef _FAM_RDMA_H_
#define _FAM_RDMA_H_

#include <vector>
#include <memory>
#include <rdma/rdma_cma.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <type_traits>
#include <cstring>
#include <thread>
#include <atomic>

#include <FAM_segment.hpp>
#include "util.hpp"
#include "FAM.hpp"

namespace FAM {
namespace rdma {
  struct EventChannelDeleter
  {
    void operator()(rdma_event_channel *c) noexcept
    {
      rdma_destroy_event_channel(c);
    }
  };

  struct RdmaIdDeleter
  {
    void operator()(rdma_cm_id *id) noexcept
    {
      rdma_disconnect(id);
      rdma_destroy_qp(id);
      rdma_destroy_id(id);
    }
  };

  auto inline CreateEventChannel()
  {
    auto channel = rdma_create_event_channel();
    if (!channel) {
      throw std::runtime_error("rdma_create_event_channel() failed");
    }
    EventChannelDeleter del;
    return std::unique_ptr<std::remove_pointer<decltype(channel)>::type,
      decltype(del)>(channel, del);
  }

  auto inline CreateRdmaId(rdma_event_channel *const channel)
  {
    rdma_cm_id *id;
    auto err = rdma_create_id(channel, &id, nullptr, RDMA_PS_TCP);
    if (err) { throw std::runtime_error("rdma_create_id() failed"); }
    RdmaIdDeleter del;
    return std::unique_ptr<std::remove_pointer<decltype(id)>::type,
      decltype(del)>(id, del);
  }

  void inline bind_addr(rdma_cm_id *const id, const uint64_t memserver_port)
  {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(memserver_port));
    auto const err =
      rdma_bind_addr(id, reinterpret_cast<struct sockaddr *>(&addr));
    if (err) throw std::runtime_error("rdma_bind_addr() failed");
  }

  void inline listen(rdma_cm_id *const id)
  {
    constexpr int backlog = 10;
    auto const err = rdma_listen(id, backlog);
    if (err) throw std::runtime_error("rdma_listen() failed");
  }

  int inline HCA_responder_resources() { return 0; }

  auto inline RdmaConnParams() noexcept
  {
    struct rdma_conn_param params;
    std::memset(&params, 0, sizeof(params));
    params.responder_resources = 1;
    params.initiator_depth = 1;
    params.rnr_retry_count = 7;

    return params;
  }

  class RdmaMemoryBuffer
  {
  public:
    std::uint64_t size;
    std::unique_ptr<void, std::function<void(void *)>> p;
    ibv_mr *mr;

    RdmaMemoryBuffer(rdma_cm_id *id,
      std::uint64_t const t_size,
      bool const use_HP,
      bool const write_allowed);

    RdmaMemoryBuffer(RdmaMemoryBuffer &&) = delete;
    RdmaMemoryBuffer &operator=(RdmaMemoryBuffer &&) = delete;

    ~RdmaMemoryBuffer();
  };

  void PollCompletionQueue(
    std::vector<std::unique_ptr<rdma_cm_id, FAM::rdma::RdmaIdDeleter>> &cm_ids,
    std::atomic<bool> &keep_spinning);


}// namespace rdma
struct IbWorkRequest;

}// namespace FAM

class FAM::FamControl::RdmaServiceImpl
{
  decltype(FAM::rdma::CreateEventChannel()) ec;
  std::string host;
  std::string port;
  std::vector<std::unique_ptr<FAM::rdma::RdmaMemoryBuffer>> regions;
  std::vector<decltype(FAM::rdma::CreateRdmaId(ec.get()))> ids;
  std::thread poller;
  std::atomic<bool> keep_spinning = true;
  std::vector<std::unique_ptr<IbWorkRequest[]>> wrs;

  void CreateConnection();

public:
  RdmaServiceImpl(std::string const &t_host,
    std::string const &t_port,
    int const channels);

  ~RdmaServiceImpl();

  RdmaServiceImpl(const RdmaServiceImpl &) = delete;
  RdmaServiceImpl &operator=(const RdmaServiceImpl &) = delete;

  std::pair<void *, uint32_t> CreateRegion(std::uint64_t const t_size,
    bool const use_HP,
    bool const write_allowed);

  void Read(uint64_t laddr,
    uint64_t raddr,
    uint32_t length,
    uint32_t lkey,
    uint32_t rkey,
    unsigned long channel) noexcept;

  void Read(uint64_t laddr,
    std::vector<FamSegment> const &segs,
    uint32_t lkey,
    uint32_t rkey,
    unsigned long channel) noexcept;

  void Write(uint64_t laddr,
    uint64_t raddr,
    uint32_t length,
    uint32_t lkey,
    uint32_t rkey,
    unsigned long channel) noexcept;
};

#endif
