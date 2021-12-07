#ifndef _FAM_RDMA_H_
#define _FAM_RDMA_H_

#include <vector>
#include <memory>
#include <rdma/rdma_cma.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <type_traits>
#include <cstring>

// #include <spdlog/spdlog.h> //DELETE

namespace FAM {
namespace RDMA {
  struct ec_deleter
  {
    void operator()(rdma_event_channel *c) { rdma_destroy_event_channel(c); }
  };

  struct id_deleter
  {
    void operator()(rdma_cm_id *id)
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

  class client
  {
    decltype(FAM::RDMA::create_ec()) ec;
    std::string host;
    std::string port;

  public:
    std::vector<decltype(create_id(ec.get()))> ids;

    client(std::string const &t_host, std::string const &t_port)
      : ec{ create_ec() }, host{ t_host }, port{ t_port }
    {}

    client(const client &) = delete;
    client &operator=(const client &) = delete;

    void create_connection();
  };

}// namespace RDMA
}// namespace FAM

#endif
