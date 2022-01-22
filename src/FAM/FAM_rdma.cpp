#include <FAM.hpp>
#include "FAM_rdma.hpp"
#include <FAM_constants.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rdma/rdma_verbs.h>
#include <utility>

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

void FAM::client::FAM_control::RDMA_service_impl::create_connection()
{
  auto chan = ec.get();
  auto t_id = FAM::RDMA::create_id(chan);
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

std::pair<void *, uint32_t>
  FAM::client::FAM_control::RDMA_service_impl::create_region(
    std::uint64_t const t_size,
    bool const use_HP,
    bool const write_allowed)
{
  if (this->ids.size() == 0) throw std::runtime_error("No rdma_cm_id's to use");
  auto id = this->ids.front().get();
  this->regions.emplace_back(
    std::make_unique<FAM::RDMA::RDMA_mem>(id, t_size, use_HP, write_allowed));
  auto const p = this->regions.back()->p.get();
  auto const lkey = this->regions.back()->mr->lkey;
  return std::make_pair(p, lkey);
}

struct FAM::WR
{
  struct ibv_send_wr wr;
  struct ibv_sge sge;
};

FAM::client::FAM_control::RDMA_service_impl::RDMA_service_impl(
  std::string const &t_host,
  std::string const &t_port,
  int const channels)
  : ec{ FAM::RDMA::create_ec() }, host{ t_host }, port{ t_port }
{
  for (int i = 0; i < channels; ++i) {
    this->create_connection();
    this->wrs.emplace_back(
      std::unique_ptr<WR[]>(new WR[FAM::max_outstanding_wr]));
  }
  this->poller = std::thread(
    FAM::RDMA::poll_cq, std::ref(this->ids), std::ref(this->keep_spinning));
}

FAM::client::FAM_control::RDMA_service_impl::~RDMA_service_impl()
{
  this->keep_spinning = false;
  this->poller.join();
}

namespace {
void prep_wr(FAM::WR &t_wr,
  uint64_t laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  ibv_wr_opcode op,
  ibv_send_flags flags,
  ibv_send_wr *next) noexcept
{
  ibv_send_wr &wr = t_wr.wr;
  ibv_sge &sge = t_wr.sge;
  memset(&wr, 0, sizeof(wr));// maybe optimize away

  wr.opcode = op;
  wr.send_flags = flags;
  wr.wr.rdma.remote_addr = raddr;
  wr.wr.rdma.rkey = rkey;
  wr.next = next;

  wr.sg_list = &sge;
  wr.num_sge = 1;
  sge.addr = laddr;
  sge.length = length;
  sge.lkey = lkey;

  //  return std::make_pair(wr, sge);
}

void prep_wrs() {}

}// namespace


void FAM::client::FAM_control::RDMA_service_impl::read(uint64_t laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel) noexcept
{
  auto id = this->ids[channel].get();
  auto &wr = this->wrs[channel][0];
  prep_wr(wr,
    laddr,
    raddr,
    length,
    lkey,
    rkey,
    IBV_WR_RDMA_READ,
    IBV_SEND_SIGNALED,
    nullptr);
  struct ibv_send_wr *bad_wr = nullptr;

  auto ret = ibv_post_send(id->qp, &wr.wr, &bad_wr);
  if (ret) spdlog::error("ibv_post_send() failed (read)");
}

void FAM::client::FAM_control::RDMA_service_impl::read(uint64_t laddr,
  std::vector<FAM::FAM_segment> const &segs,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel) noexcept
{
  // auto id = this->ids[channel].get();
  // auto [wr, sge] = prep_wr(
  //   laddr, raddr, length, lkey, rkey, IBV_WR_RDMA_READ, IBV_SEND_SIGNALED);
  // struct ibv_send_wr *bad_wr = nullptr;

  // auto ret = ibv_post_send(id->qp, &wr, &bad_wr);
  // if (ret) spdlog::error("ibv_post_send() failed (read)");
}

void FAM::client::FAM_control::RDMA_service_impl::write(uint64_t laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel) noexcept
{
  auto id = this->ids[channel].get();
  auto &wr = this->wrs[channel][0];
  prep_wr(wr,
    laddr,
    raddr,
    length,
    lkey,
    rkey,
    IBV_WR_RDMA_WRITE,
    IBV_SEND_SIGNALED,
    nullptr);
  struct ibv_send_wr *bad_wr = nullptr;

  auto ret = ibv_post_send(id->qp, &wr.wr, &bad_wr);
  if (ret) spdlog::error("ibv_post_send() failed (write)");
}

FAM::RDMA::RDMA_mem::RDMA_mem(rdma_cm_id *id,
  std::uint64_t const t_size,
  bool const use_HP,
  bool const write_allowed)
  : size{ t_size }, p{ FAM::Util::mmap(t_size, use_HP) }
{
  spdlog::debug("RDMA_mem()");
  auto ptr = p.get();
  this->mr = [=]() {
    return ibv_reg_mr(id->pd,
      ptr,
      t_size,
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE
        | IBV_ACCESS_REMOTE_READ);
    // if (write_allowed)
    //   return rdma_reg_write(id, ptr, t_size);
    // else
    //   return rdma_reg_read(id, ptr, t_size);
  }();

  if (!this->mr) throw std::runtime_error("rdma_reg() failed!");
}

FAM::RDMA::RDMA_mem::~RDMA_mem()
{
  spdlog::debug("~RDMA_mem()");
  if (rdma_dereg_mr(this->mr)) spdlog::error("rdma_dereg failed");
}

void FAM::RDMA::poll_cq(
  std::vector<std::unique_ptr<rdma_cm_id, FAM::RDMA::id_deleter>> &cm_ids,
  std::atomic<bool> &keep_spinning)
{
  constexpr auto k = 10;
  struct ibv_cq *cq;
  struct ibv_wc wc[k];
  constexpr unsigned long batch = 1 << 10;

  while (keep_spinning) {
    for (unsigned long iter = 0; iter < batch; ++iter) {
      for (auto &id : cm_ids) {
        cq = id->send_cq;
        if (int n = ibv_poll_cq(cq, k, wc)) {
          for (int i = 0; i < n; ++i) {
            if (wc[i].status != IBV_WC_SUCCESS) {
              spdlog::error("poll_cq: Completion Status is not success");
              throw std::runtime_error("ibv_poll_cq() failed");
            }
          }
        }
      }
    }
  }
}
