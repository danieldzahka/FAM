#include <FAM_rdma.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace {
constexpr int TIMEOUT_MS = 500;

auto get_event(rdma_event_channel *chan)
{
  struct rdma_cm_event *event = nullptr;
  if (rdma_get_cm_event(chan, &event) == 0) {
    struct rdma_cm_event event_copy;
    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);
    return event_copy;
  }
  throw std::runtime_error("rdma_get_cm_event() failed!");
}

void resolve_addr(rdma_cm_id *id,
  std::string const &host,
  std::string const &port)
{
  struct addrinfo *addr;
  getaddrinfo(host.c_str(), port.c_str(), NULL, &addr);
  if (rdma_resolve_addr(id, NULL, addr->ai_addr, TIMEOUT_MS)) {
    throw std::runtime_error("rdma_resolve_addr() failed!");
  }
  freeaddrinfo(addr);
}

auto create_qp_attr() noexcept
{
  struct ibv_qp_init_attr qp_attr;
  memset(&qp_attr, 0, sizeof(qp_attr));
  qp_attr.qp_type = IBV_QPT_RC;
  qp_attr.cap.max_send_wr = 10000;// increase later
  qp_attr.cap.max_recv_wr = 10;
  qp_attr.cap.max_send_sge = 1;
  qp_attr.cap.max_recv_sge = 1;
  qp_attr.sq_sig_all = 0;// shouldn't need this explicitly

  return qp_attr;
}

void connect(rdma_cm_id *id, rdma_conn_param &params)
{
  if (rdma_connect(id, &params))
    throw std::runtime_error("rdma_connect() failed!");
}

void create_qp(rdma_cm_id *id, ibv_qp_init_attr &qp_attr)
{
  if (rdma_create_qp(id, nullptr, &qp_attr))
    throw std::runtime_error("rdma_create_qp() failed!");
}
}// namespace

void FAM::RDMA::client::create_connection()
{
  auto chan = ec.get();
  auto t_id = create_id(chan);
  auto id = t_id.get();

  resolve_addr(id, this->host, this->port);

  auto event = get_event(chan);
  if (event.event != RDMA_CM_EVENT_ADDR_RESOLVED)
    throw std::runtime_error("rdma_resolve_addr() failed!");

  auto qp_attr = create_qp_attr();
  create_qp(id, qp_attr);
  if (rdma_resolve_route(id, TIMEOUT_MS))
    throw std::runtime_error("rdma_resolve_route() failed!");

  auto event2 = get_event(chan);
  if (event2.event != RDMA_CM_EVENT_ROUTE_RESOLVED)
    throw std::runtime_error("rdma_resolve_addr() failed!");

  auto params = FAM::RDMA::get_cm_params();
  connect(id, params);

  auto event3 = get_event(chan);
  if (event3.event != RDMA_CM_EVENT_ESTABLISHED)
    throw std::runtime_error("connection not established");

  this->ids.push_back(std::move(t_id));
}
