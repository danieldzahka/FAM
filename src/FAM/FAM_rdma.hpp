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

#include "util.hpp"
#include <spdlog/spdlog.h>

namespace FAM {
namespace RDMA {
  struct ec_deleter
  {
    void operator()(rdma_event_channel *c) noexcept
    {
      rdma_destroy_event_channel(c);
    }
  };

  struct id_deleter
  {
    void operator()(rdma_cm_id *id) noexcept
    {
      rdma_disconnect(id);
      rdma_destroy_qp(id);
      rdma_destroy_id(id);
    }
  };

  auto inline create_ec()
  {
    auto channel = rdma_create_event_channel();
    if (!channel) {
      throw std::runtime_error("rdma_create_event_channel() failed");
    }
    ec_deleter del;
    return std::unique_ptr<std::remove_pointer<decltype(channel)>::type,
      decltype(del)>(channel, del);
  }

  auto inline create_id(rdma_event_channel *const channel)
  {
    rdma_cm_id *id;
    auto err = rdma_create_id(channel, &id, nullptr, RDMA_PS_TCP);
    if (err) { throw std::runtime_error("rdma_create_id() failed"); }
    id_deleter del;
    return std::unique_ptr<std::remove_pointer<decltype(id)>::type,
      decltype(del)>(id, del);
  }

  void inline bind_addr(rdma_cm_id *const id)
  {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(35287));
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

  auto inline get_cm_params() noexcept
  {
    struct rdma_conn_param params;
    std::memset(&params, 0, sizeof(params));
    params.responder_resources = 1;
    params.initiator_depth = 1;
    params.rnr_retry_count = 7;

    return params;
  }

  class RDMA_mem
  {
  public:
    std::uint64_t size;
    std::unique_ptr<void, std::function<void(void *)>> p;
    ibv_mr *mr;

    RDMA_mem(rdma_cm_id *id,
      std::uint64_t const t_size,
      bool const use_HP,
      bool const write_allowed);

    RDMA_mem(RDMA_mem &&) = delete;
    RDMA_mem &operator=(RDMA_mem &&) = delete;

    ~RDMA_mem();
  };

  void poll_cq(
    std::vector<std::unique_ptr<rdma_cm_id, FAM::RDMA::id_deleter>> &cm_ids,
    std::atomic<bool> &keep_spinning) noexcept;


}// namespace RDMA
}// namespace FAM

class FAM::client::FAM_control::RDMA_service_impl
{
  decltype(FAM::RDMA::create_ec()) ec;
  std::string host;
  std::string port;
  std::vector<std::unique_ptr<FAM::RDMA::RDMA_mem>> regions;
  std::vector<decltype(FAM::RDMA::create_id(ec.get()))> ids;
  std::thread poller;
  std::atomic<bool> keep_spinning = true;

  void create_connection();

public:
  RDMA_service_impl(std::string const &t_host,
    std::string const &t_port,
    int const channels)
    : ec{ FAM::RDMA::create_ec() }, host{ t_host }, port{ t_port }
  {
    for (int i = 0; i < channels; ++i) this->create_connection();
    this->poller = std::thread(
      FAM::RDMA::poll_cq, std::ref(this->ids), std::ref(this->keep_spinning));
  }

  ~RDMA_service_impl()
  {
    this->keep_spinning = false;
    this->poller.join();
    spdlog::info("joined comm thread");
  }

  RDMA_service_impl(const RDMA_service_impl &) = delete;
  RDMA_service_impl &operator=(const RDMA_service_impl &) = delete;  

  void *create_region(std::uint64_t const t_size,
    bool const use_HP,
    bool const write_allowed);

  void read(void *laddr, void *raddr, uint32_t length) noexcept;
  void write(void *laddr, void *raddr, uint32_t length) noexcept;
};

#endif
