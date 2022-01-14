#include <FAM.hpp>
#include "FAM_rdma.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rdma/rdma_verbs.h>

#include <spdlog/spdlog.h>

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

void FAM::RDMA::client_impl::create_connection()
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

void *FAM::RDMA::client_impl::create_region(std::uint64_t const t_size,
  bool const use_HP,
  bool const write_allowed)
{
  if (this->ids.size() == 0) throw std::runtime_error("No rdma_cm_id's to use");
  auto id = this->ids.front().get();
  this->regions.emplace_back(id, t_size, use_HP, write_allowed);
  auto p = this->regions.back().p.get();
  return p;
}

void FAM::RDMA::client_impl::read(void *laddr,
  void *raddr,
  uint32_t length) noexcept
{
  // struct ibv_send_wr wr;
  // struct ibv_sge sge;
  // memset(&wr, 0, sizeof(wr));// maybe optimize away

  // wr.opcode = IBV_WR_RDMA_READ;
  // wr.send_flags = IBV_SEND_SIGNALED;// can change for selective signaling
  // wr.wr.rdma.remote_addr = ;
  // wr.wr.rdma.rkey = ;

  // wr.sg_list = &sge;
  // wr.num_sge = 1;
  // sge.addr = reinterpret_cast<uintptr_t>(buffer);
  // sge.length = length;
  // sge.lkey = ctx->heap_mr->lkey;
}

void FAM::RDMA::client_impl::write(void *laddr,
  void *raddr,
  uint32_t length) noexcept
{}


FAM::RDMA::client::client(std::string const &t_host, std::string const &t_port)
  : pimpl{ std::make_unique<FAM::RDMA::client_impl>(t_host, t_port) }
{}

FAM::RDMA::client::~client() = default;

void FAM::RDMA::client::create_connection()
{
  this->pimpl->create_connection();
}

void *FAM::RDMA::client::create_region(std::uint64_t const t_size,
  bool const use_HP,
  bool const write_allowed)
{
  return this->pimpl->create_region(t_size, use_HP, write_allowed);
}

void FAM::RDMA::client::read(void *laddr, void *raddr, uint32_t length) noexcept
{
  this->pimpl->read(laddr, raddr, length);
}

void FAM::RDMA::client::write(void *laddr,
  void *raddr,
  uint32_t length) noexcept
{
  this->pimpl->write(laddr, raddr, length);
}

FAM::RDMA::RDMA_mem::RDMA_mem(rdma_cm_id *id,
  std::uint64_t const t_size,
  bool const use_HP,
  bool const write_allowed)
  : size{ t_size }, p{ FAM::Util::mmap(t_size, use_HP) }
{
  auto ptr = p.get();
  this->mr = [=]() {
    if (write_allowed)
      return rdma_reg_write(id, ptr, t_size);
    else
      return rdma_reg_read(id, ptr, t_size);
  }();

  if (!this->mr) throw std::runtime_error("rdma_reg() failed!");
}

FAM::RDMA::RDMA_mem::~RDMA_mem()
{
  if (rdma_dereg_mr(this->mr)) spdlog::error("rdma_dereg failed");
}
