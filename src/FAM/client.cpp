#include <FAM.hpp>

#include <stdexcept>
#include <string>
#include <iostream>
#include <memory>
#include <cassert>

#include <grpcpp/grpcpp.h>

#include "fam.grpc.pb.h"
#include "FAM_rdma.hpp"
#include <FAM_constants.hpp>

#include <spdlog/spdlog.h>

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

    if (status.ok())
      return std::make_tuple(reply.addr(), reply.length(), reply.rkey());

    throw std::runtime_error(status.error_message());
  }

  auto MmapFile(std::string const &filepath)
  {
    fam::MmapFileRequest request;
    request.set_path(filepath);
    fam::MmapFileReply reply;
    ClientContext context;
    auto const status = stub_->MmapFile(&context, request, &reply);

    if (status.ok())
      return std::make_tuple(reply.addr(), reply.length(), reply.rkey());

    throw std::runtime_error(status.error_message());
  }

  void EndSession()
  {
    fam::EndSessionRequest request;
    fam::EndSessionReply reply;
    ClientContext context;

    Status status = stub_->EndSession(&context, request, &reply);
    if (status.ok()) return;
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

FAM::client::FAM_control::~FAM_control()
{
  try {
    this->control_service->EndSession();
  } catch (std::exception const &e) {
    spdlog::error("Error in ~FAM_control(): {}", e.what());
  }
}

void FAM::client::FAM_control::ping() { this->control_service->Ping(); }

FAM::client::FAM_control::remote_region
  FAM::client::FAM_control::allocate_region(std::uint64_t size)
{
  auto const [addr, length, rkey] = this->control_service->AllocateRegion(size);
  return FAM::client::FAM_control::remote_region{ addr, length, rkey };
}

FAM::client::FAM_control::remote_region
  FAM::client::FAM_control::mmap_remote_file(std::string const &filepath)
{
  auto const [addr, length, rkey] = this->control_service->MmapFile(filepath);
  return FAM::client::FAM_control::remote_region{ addr, length, rkey };
}


FAM::client::FAM_control::local_region FAM::client::FAM_control::create_region(
  const std::uint64_t t_size,
  const bool use_HP,
  const bool write_allowed)
{
  auto const [addr, lkey] =
    this->RDMA_service->create_region(t_size, use_HP, write_allowed);
  auto const length = t_size;
  return FAM::client::FAM_control::local_region{ addr, length, lkey };
}

void FAM::client::FAM_control::read(void *laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel = 0) noexcept
{
  this->RDMA_service->read(
    reinterpret_cast<uint64_t>(laddr), raddr, length, lkey, rkey, channel);
}

void FAM::client::FAM_control::read(void *laddr,
  std::vector<FAM::FAM_segment> const &segs,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel) noexcept
{
  //uphold the narrow calling contract
  assert(segs.size() <= FAM::max_outstanding_wr);
  this->RDMA_service->read(
    reinterpret_cast<uint64_t>(laddr), segs, lkey, rkey, channel);
}


void FAM::client::FAM_control::write(void *laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel = 0) noexcept
{
  this->RDMA_service->write(
    reinterpret_cast<uint64_t>(laddr), raddr, length, lkey, rkey, channel);
}
