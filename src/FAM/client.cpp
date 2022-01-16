#include <FAM.hpp>

#include <stdexcept>
#include <string>
#include <iostream>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "fam.grpc.pb.h"
#include "FAM_rdma.hpp"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using fam::FAMController;

class FAM::client::FAM_control::control_service_impl
{
public:
  control_service_impl(std::shared_ptr<Channel> channel)
    : stub_(FAMController::NewStub(channel))
  {}

  void Ping()
  {
    fam::PingRequest request;
    fam::PingReply reply;
    ClientContext context;

    Status status = stub_->Ping(&context, request, &reply);
    if (status.ok()) return;
    throw std::runtime_error(status.error_message());
  }

  auto AllocateRegion(std::uint64_t const size)
  {
    fam::AllocateRegionRequest request;
    request.set_size(size);
    fam::AllocateRegionReply reply;
    ClientContext context;
    auto const status = stub_->AllocateRegion(&context, request, &reply);

    if (status.ok()) return std::make_pair(reply.addr(), reply.length());

    throw std::runtime_error(status.error_message());
  }

private:
  std::unique_ptr<FAMController::Stub> stub_;
};

FAM::client::FAM_control::FAM_control(std::string const &control_addr,
  std::string const &RDMA_addr,
  std::string const &RDMA_port,
  int const rdma_channels)
  : control_service{ std::make_unique<
      FAM::client::FAM_control::FAM_control::control_service_impl>(
      grpc::CreateChannel(control_addr, grpc::InsecureChannelCredentials())) },
    RDMA_service{ std::make_unique<FAM::client::FAM_control::RDMA_service_impl>(
      RDMA_addr,
      RDMA_port,
      rdma_channels) }

{}

FAM::client::FAM_control::~FAM_control() {}

void FAM::client::FAM_control::ping() { this->control_service->Ping(); }

FAM::client::FAM_control::remote_region
  FAM::client::FAM_control::allocate_region(std::uint64_t size)
{
  auto const [addr, length] = this->control_service->AllocateRegion(size);
  return FAM::client::FAM_control::remote_region{ addr, length };
}

// void FAM::client::FAM_control::create_connection()
// {
//   this->RDMA_service->create_connection();
// }

FAM::client::FAM_control::local_region FAM::client::FAM_control::create_region(
  const std::uint64_t t_size,
  const bool use_HP,
  const bool write_allowed)
{
  auto addr = this->RDMA_service->create_region(t_size, use_HP, write_allowed);
  auto length = t_size;
  return FAM::client::FAM_control::local_region{ addr, length };
}
