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

class FAM::client::FamControl::ControlServiceImpl
{
public:
  ControlServiceImpl(std::shared_ptr<Channel> channel)
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

FAM::client::FamControl::FamControl(std::string const &control_addr,
  std::string const &RDMA_addr,
  std::string const &RDMA_port,
  int const rdma_channels)
  : control_service{ std::make_unique<
    FAM::client::FamControl::FamControl::ControlServiceImpl>(
    grpc::CreateChannel(control_addr, grpc::InsecureChannelCredentials())) },
    RDMA_service{ std::make_unique<FAM::client::FamControl::RdmaServiceImpl>(
      RDMA_addr,
      RDMA_port,
      rdma_channels) }

{}

FAM::client::FamControl::~FamControl()
{
  try {
    this->control_service->EndSession();
  } catch (std::exception const &e) {
    spdlog::error("Error in FamControl): {}", e.what());
  }
}

void FAM::client::FamControl::ping() { this->control_service->Ping(); }

FAM::client::FamControl::RemoteRegion FAM::client::FamControl::AllocateRegion(
  std::uint64_t size)
{
  auto const [addr, length, rkey] = this->control_service->AllocateRegion(size);
  return FAM::client::FamControl::RemoteRegion{ addr, length, rkey };
}

FAM::client::FamControl::RemoteRegion FAM::client::FamControl::MmapRemoteFile(
  std::string const &filepath)
{
  auto const [addr, length, rkey] = this->control_service->MmapFile(filepath);
  return FAM::client::FamControl::RemoteRegion{ addr, length, rkey };
}


FAM::client::FamControl::LocalRegion FAM::client::FamControl::CreateRegion(
  const std::uint64_t t_size,
  const bool use_HP,
  const bool write_allowed)
{
  auto const [addr, lkey] =
    this->RDMA_service->CreateRegion(t_size, use_HP, write_allowed);
  auto const length = t_size;
  return FAM::client::FamControl::LocalRegion{ addr, length, lkey };
}

void FAM::client::FamControl::Read(void *laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel = 0) noexcept
{
  this->RDMA_service->Read(
    reinterpret_cast<uint64_t>(laddr), raddr, length, lkey, rkey, channel);
}

void FAM::client::FamControl::Read(void *laddr,
  std::vector<FAM::FamSegment> const &segs,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel) noexcept
{
  // uphold the narrow calling contract
  assert(segs.size() <= FAM::max_outstanding_wr);
  this->RDMA_service->Read(
    reinterpret_cast<uint64_t>(laddr), segs, lkey, rkey, channel);
}


void FAM::client::FamControl::Write(void *laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel = 0) noexcept
{
  this->RDMA_service->Write(
    reinterpret_cast<uint64_t>(laddr), raddr, length, lkey, rkey, channel);
}
