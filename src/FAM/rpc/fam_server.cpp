/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include <fam.grpc.pb.h>

namespace {

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;
using fam::Greeter;
using fam::HelloReply;
using fam::HelloRequest;



  
class ServerImpl final
{
public:
  ~ServerImpl()
  {
    server_->Shutdown();
    cq_->Shutdown();
  }

  // There is no shutdown handling in this code.
  void Run()
  {
    std::string server_address("0.0.0.0:50051");

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    cq_ = builder.AddCompletionQueue();

    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;
    HandleRpcs();
  }

private:
  class async_state_machine
  {
  public:
    async_state_machine(Greeter::AsyncService *service,
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
    Greeter::AsyncService *service_;
    ServerCompletionQueue *cq_;
    ServerContext ctx_;
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;
  };

  class Hello : public async_state_machine
  {
    HelloRequest request_;
    HelloReply reply_;
    ServerAsyncResponseWriter<HelloReply> responder_;

  public:
    Hello(Greeter::AsyncService *service, ServerCompletionQueue *cq)
      : async_state_machine(service, cq), responder_(&ctx_)
    {}

    void request() override
    {
      service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_, this);
    };
    void handle() override
    {
      (new Hello(service_, cq_))->Proceed();
      std::string prefix("Hello ");
      reply_.set_message(prefix + request_.name());
      status_ = FINISH;
      responder_.Finish(reply_, Status::OK, this);
    }
  };

  class Other : public async_state_machine
  {
    fam::otherRequest request_;
    fam::otherReply reply_;
    ServerAsyncResponseWriter<fam::otherReply> responder_;

  public:
    Other(Greeter::AsyncService *service, ServerCompletionQueue *cq)
      : async_state_machine(service, cq), responder_(&ctx_)
    {}

    void request() override
    {
      service_->RequestSayother(&ctx_, &request_, &responder_, cq_, cq_, this);
    };
    void handle() override
    {
      (new Other(service_, cq_))->Proceed();
      std::string prefix("Other ");
      reply_.set_message(prefix + request_.name());
      status_ = FINISH;
      responder_.Finish(reply_, Status::OK, this);
    }
  };

  void HandleRpcs()
  {
    // Spawn a new CallData instance to serve new clients.
    (new Hello(&service_, cq_.get()))->Proceed();
    (new Other(&service_, cq_.get()))->Proceed();
    void *tag;// uniquely identifies a request.
    bool ok;
    while (true) {
      GPR_ASSERT(cq_->Next(&tag, &ok));
      GPR_ASSERT(ok);
      static_cast<async_state_machine *>(tag)->Proceed();
    }
  }

  std::unique_ptr<ServerCompletionQueue> cq_;
  Greeter::AsyncService service_;
  std::unique_ptr<Server> server_;
};
}// namespace

int main(int, char **)
{
  ServerImpl server;
  server.Run();

  return 0;
}
