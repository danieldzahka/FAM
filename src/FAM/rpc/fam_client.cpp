#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "fam.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using fam::Greeter;
using fam::HelloReply;
using fam::HelloRequest;

class GreeterClient
{
public:
  GreeterClient(std::shared_ptr<Channel> channel)
    : stub_(Greeter::NewStub(channel))
  {}

  std::string SayHello(const std::string &user)
  {
    HelloRequest request;
    request.set_name(user);
    HelloReply reply;
    ClientContext context;

    Status status = stub_->SayHello(&context, request, &reply);
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

  std::string Sayother(const std::string &user)
  {
    fam::otherRequest request;
    request.set_name(user);
    fam::otherReply reply;
    ClientContext context;
    Status status = stub_->Sayother(&context, request, &reply);
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

private:
  std::unique_ptr<Greeter::Stub> stub_;
};

int main(int, char **)
{
  std::string target_str = "localhost:50051";

  GreeterClient greeter(
    grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  std::string user("world");
  std::string reply = greeter.SayHello(user);
  std::cout << "Greeter received: " << reply << std::endl;

  reply = greeter.Sayother(user);
  std::cout << "Greeter received: " << reply << std::endl;

  return 0;
}
