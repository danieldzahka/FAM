#include <FAM.hpp>
#include "FAM_rdma.hpp"
#include "util.hpp"

#include <cstdlib>
#include <functional>
#include <string>
#include <thread>
#include <cstring>
#include <poll.h>

#include <memory>
#include <chrono>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include <fam.grpc.pb.h>

#include <spdlog/spdlog.h>//delete maybe

namespace {

class session
{
public:
  rdma_cm_id *const id;
  std::vector<std::unique_ptr<FAM::rdma::RdmaMemoryBuffer>> client_regions;

  session(rdma_cm_id *t_id) : id{ t_id } {}

  session &operator=(const session &) = delete;
  session(const session &) = delete;
};

void rdma_handle_event(rdma_cm_event const &event_copy, session &s)
{
  spdlog::debug(rdma_event_str(event_copy.event));

  if (event_copy.event == RDMA_CM_EVENT_CONNECT_REQUEST) {// Runs on server
    struct ibv_qp_init_attr qp_attr;
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 10000;// increase later
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.sq_sig_all = 0;// shouldn't need this explicitly
    if (rdma_create_qp(event_copy.id, nullptr, &qp_attr))
      throw std::runtime_error("rdma_create_qp() failed!");

    auto params = FAM::rdma::RdmaConnParams();
    auto const err = rdma_accept(event_copy.id, &params);
    if (err) throw std::runtime_error("rdma_accept() failed!");

    // spdlog::debug("id {} id->verbs {} id->pd {}",
    //   (void *)(event_copy.id),
    //   (void *)(event_copy.id)->verbs,
    //   (void *)(event_copy.id)->pd);

    s.id->pd = event_copy.id->pd;// ->pd;// grab this if null in session
  } else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED) {// Runs on both

  } else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED) {// Runs on both
    rdma_disconnect(event_copy.id);
    rdma_destroy_qp(event_copy.id);
    rdma_destroy_id(event_copy.id);
  } else {
    auto error = fmt::format(
      "Unhandled rdma_cm event {}", rdma_event_str(event_copy.event));
    throw std::runtime_error(error);
  }
}

void rdma_server_loop(rdma_event_channel *const ec, session &s)
{
  struct rdma_cm_event *event = NULL;

  pollfd pfd;
  nfds_t const n_fds = 1;
  int const ms = 0;
  pfd.fd = ec->fd;
  pfd.events = POLLIN | POLLOUT | POLLPRI;

  auto ret = poll(&pfd, n_fds, ms);

  if (ret == -1) throw std::runtime_error("poll returned -1");

  if (ret > 0) {
    if (rdma_get_cm_event(ec, &event) == 0) {
      struct rdma_cm_event event_copy;
      memcpy(&event_copy, event, sizeof(*event));
      rdma_ack_cm_event(event);
      rdma_handle_event(event_copy, s);
    } else {
      throw std::runtime_error("rdma_get_cm_event returnd non-zero");
    }
  }
}

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;
using fam::FAMController;

class ServerImpl final
{
public:
  ~ServerImpl()
  {
    server_->Shutdown();
    cq_->Shutdown();
  }

  // There is no shutdown handling in this code.
  void Run(std::string const &server_address, const uint64_t memserver_port)
  {
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    cq_ = builder.AddCompletionQueue();

    server_ = builder.BuildAndStart();
    spdlog::info("Server listening on {}", server_address);

    auto ec = FAM::rdma::CreateEventChannel();
    auto id = FAM::rdma::CreateRdmaId(ec.get());
    FAM::rdma::bind_addr(id.get(), memserver_port);
    FAM::rdma::listen(id.get());
    auto const rdma_port = ntohs(rdma_get_src_port(id.get()));
    auto addr = rdma_get_local_addr(id.get());
    char *ip = inet_ntoa(reinterpret_cast<sockaddr_in *>(addr)->sin_addr);
    spdlog::debug("Server listening on IPoIB: {}:{}", ip, rdma_port);

    session s{ id.get() };

    // spdlog::debug("listen id {} id->verbs {} id->pd {}",
    //   (void *)(s.id),
    //   (void *)(s.id)->verbs,
    //   (void *)(s.id)->pd);

    HandleRpcs(ec.get(), s);
  }

private:
  class async_state_machine
  {
  public:
    async_state_machine(FAMController::AsyncService *service,
      ServerCompletionQueue *cq)
      : service_(service), cq_(cq), status_(CREATE)
    {}

    virtual ~async_state_machine() = default;

    virtual void request() = 0;
    virtual void handle() = 0;

    void Proceed()
    {
      if (status_ == CREATE) {
        status_ = PROCESS;
        request();
      } else if (status_ == PROCESS) {
        handle();
      } else {
        GPR_ASSERT(status_ == FINISH);
        delete this;
      }
    }

  protected:
    FAMController::AsyncService *service_;
    ServerCompletionQueue *cq_;
    ServerContext ctx_;
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;
  };

  class AllocateRegionHandler : public async_state_machine
  {
    fam::AllocateRegionRequest request_;
    fam::AllocateRegionReply reply_;
    ServerAsyncResponseWriter<fam::AllocateRegionReply> responder_;
    session &s;

  public:
    AllocateRegionHandler(FAMController::AsyncService *service,
      ServerCompletionQueue *cq,
      session &t_s)
      : async_state_machine(service, cq), responder_(&ctx_), s{ t_s }
    {}

    void request() override
    {
      service_->RequestAllocateRegion(
        &ctx_, &request_, &responder_, cq_, cq_, this);
    };
    void handle() override
    {
      (new AllocateRegionHandler(service_, cq_, s))->Proceed();

      auto const length = request_.size();
      try {
        s.client_regions.push_back(
          std::make_unique<FAM::rdma::RdmaMemoryBuffer>(
            s.id, length, false, true));
        auto const ptr =
          reinterpret_cast<uint64_t>(s.client_regions.back()->p.get());
        auto const rkey = s.client_regions.back()->mr->rkey;
        reply_.set_addr(ptr);
        reply_.set_length(length);
        reply_.set_rkey(rkey);

        responder_.Finish(reply_, Status::OK, this);
      } catch (std::exception const &e) {
        responder_.Finish(reply_,
          Status(grpc::StatusCode::ABORTED, "rdma region create failed"),
          this);
        spdlog::error("Region creation failed!");
      }
      status_ = FINISH;
    }
  };

  class MmapFileHandler : public async_state_machine
  {
    fam::MmapFileRequest request_;
    fam::MmapFileReply reply_;
    ServerAsyncResponseWriter<fam::MmapFileReply> responder_;
    session &s;

  public:
    MmapFileHandler(FAMController::AsyncService *service,
      ServerCompletionQueue *cq,
      session &t_s)
      : async_state_machine(service, cq), responder_(&ctx_), s{ t_s }
    {}

    void request() override
    {
      service_->RequestMmapFile(&ctx_, &request_, &responder_, cq_, cq_, this);
    };
    void handle() override
    {
      (new MmapFileHandler(service_, cq_, s))->Proceed();

      auto const filename = request_.path();
      auto const length = FAM::Util::file_size(filename);

      try {
        s.client_regions.push_back(
          std::make_unique<FAM::rdma::RdmaMemoryBuffer>(
            s.id, length, false, true));
        auto const ptr = s.client_regions.back()->p.get();
        auto const rkey = s.client_regions.back()->mr->rkey;

        FAM::Util::copy_file(ptr, filename, length);

        reply_.set_addr(reinterpret_cast<uint64_t>(ptr));
        reply_.set_length(length);
        reply_.set_rkey(rkey);

        responder_.Finish(reply_, Status::OK, this);
      } catch (std::exception const &e) {
        responder_.Finish(reply_,
          Status(grpc::StatusCode::ABORTED, "rdma region create failed"),
          this);
        spdlog::error("Region creation failed!");
      }
      status_ = FINISH;
    }
  };

  class PingHandler : public async_state_machine
  {
    fam::PingRequest request_;
    fam::PingReply reply_;
    ServerAsyncResponseWriter<fam::PingReply> responder_;

  public:
    PingHandler(FAMController::AsyncService *service, ServerCompletionQueue *cq)
      : async_state_machine(service, cq), responder_(&ctx_)
    {}

    void request() override
    {
      service_->RequestPing(&ctx_, &request_, &responder_, cq_, cq_, this);
    };
    void handle() override
    {
      (new PingHandler(service_, cq_))->Proceed();
      status_ = FINISH;
      responder_.Finish(reply_, Status::OK, this);
    }
  };

  class EndSessionHandler : public async_state_machine
  {
    fam::EndSessionRequest request_;
    fam::EndSessionReply reply_;
    ServerAsyncResponseWriter<fam::EndSessionReply> responder_;
    session &s;

  public:
    EndSessionHandler(FAMController::AsyncService *service,
      ServerCompletionQueue *cq,
      session &t_s)
      : async_state_machine(service, cq), responder_(&ctx_), s{ t_s }
    {}

    void request() override
    {
      service_->RequestEndSession(
        &ctx_, &request_, &responder_, cq_, cq_, this);
    };
    void handle() override
    {
      (new EndSessionHandler(service_, cq_, s))->Proceed();
      this->s.client_regions.clear();
      status_ = FINISH;
      responder_.Finish(reply_, Status::OK, this);
    }
  };

  void HandleRpcs(rdma_event_channel *ec, session &s)
  {
    using namespace std::literals;

    (new AllocateRegionHandler(&service_, cq_.get(), s))->Proceed();
    (new PingHandler(&service_, cq_.get()))->Proceed();
    (new EndSessionHandler(&service_, cq_.get(), s))->Proceed();
    (new MmapFileHandler(&service_, cq_.get(), s))->Proceed();
    void *tag;// uniquely identifies a request.
    bool ok;
    while (true) {
      auto const deadline = std::chrono::system_clock::now();
      auto const ret = cq_->AsyncNext(&tag, &ok, deadline);

      if (ret == grpc::CompletionQueue::NextStatus::GOT_EVENT) {
        GPR_ASSERT(ok);
        static_cast<async_state_machine *>(tag)->Proceed();
      }

      rdma_server_loop(ec, s);
    }
  }

  std::unique_ptr<ServerCompletionQueue> cq_;
  FAMController::AsyncService service_;
  std::unique_ptr<Server> server_;
};
}// namespace

void FAM::server::RunServer(std::string const &host,
  std::string const &port,
  const uint64_t memserver_port)
{
  spdlog::set_level(spdlog::level::debug);
  ServerImpl server;
  server.Run(fmt::format("{}:{}", host, port), memserver_port);
}
