#include <FAM.hpp>

#include <stdexcept>
#include <string>
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

class FAM::FamControl::ControlServiceImpl
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

FAM::FamControl::FamControl(std::string const &control_addr,
  std::string const &ipoib_addr,
  std::string const &ipoib_port,
  int const rdma_channels)
  : control_service_{ std::make_unique<
    FamControl::FamControl::ControlServiceImpl>(
    grpc::CreateChannel(control_addr, grpc::InsecureChannelCredentials())) },
    rdma_service_{ std::make_unique<FamControl::RdmaServiceImpl>(ipoib_addr,
      ipoib_port,
      rdma_channels) },
    rdma_channels_{ rdma_channels }
{}

FAM::FamControl::~FamControl()
{
  try {
    this->control_service_->EndSession();
  } catch (std::exception const &e) {
    spdlog::error("Error in FamControl): {}", e.what());
  }
}

void FAM::FamControl::Ping() { this->control_service_->Ping(); }

FAM::FamControl::RemoteRegion FAM::FamControl::AllocateRegion(
  std::uint64_t size)
{
  auto const [addr, length, rkey] =
    this->control_service_->AllocateRegion(size);
  return FamControl::RemoteRegion{ addr, length, rkey };
}

FAM::FamControl::RemoteRegion FAM::FamControl::MmapRemoteFile(
  std::string const &filepath)
{
  auto const [addr, length, rkey] = this->control_service_->MmapFile(filepath);
  return FamControl::RemoteRegion{ addr, length, rkey };
}


FAM::FamControl::LocalRegion FAM::FamControl::CreateRegion(
  const std::uint64_t t_size,
  const bool use_hugepages,
  const bool write_allowed)
{
  auto const [addr, lkey] =
    this->rdma_service_->CreateRegion(t_size, use_hugepages, write_allowed);
  auto const length = t_size;
  return FamControl::LocalRegion{ addr, length, lkey };
}

void FAM::FamControl::Read(void *laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel = 0) noexcept
{
  this->rdma_service_->Read(
    reinterpret_cast<uint64_t>(laddr), raddr, length, lkey, rkey, channel);
}

void FAM::FamControl::Read(void *laddr,
  std::vector<FAM::FamSegment> const &segs,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel) noexcept
{
  // uphold the narrow calling contract
  assert(segs.size() <= FAM::max_outstanding_wr);
  this->rdma_service_->Read(
    reinterpret_cast<uint64_t>(laddr), segs, lkey, rkey, channel);
}


void FAM::FamControl::Write(void *laddr,
  uint64_t raddr,
  uint32_t length,
  uint32_t lkey,
  uint32_t rkey,
  unsigned long channel = 0) noexcept
{
  this->rdma_service_->Write(
    reinterpret_cast<uint64_t>(laddr), raddr, length, lkey, rkey, channel);
}
