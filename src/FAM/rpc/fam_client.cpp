#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>

#include <grpcpp/grpcpp.h>

#include "fam.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using fam::FAMController;

class GreeterClient
{
public:
  GreeterClient(std::shared_ptr<Channel> channel)
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

  auto AllocateRegion(std::uint64_t const size){
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

int main(int, char **)
{
  std::string target_str = "localhost:50051";

  GreeterClient greeter(
    grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  try {
    greeter.Ping();
    std::cout << "Ping ok\n";

    auto const [addr, length] = greeter.AllocateRegion(69);
  } catch (std::exception const &e) {
    std::cout << "exception:  " << e.what() << std::endl;
  }

  return 0;
}
