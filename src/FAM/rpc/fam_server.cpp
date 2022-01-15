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

  public:
    AllocateRegionHandler(FAMController::AsyncService *service, ServerCompletionQueue *cq)
      : async_state_machine(service, cq), responder_(&ctx_)
    {}

    void request() override
    {
      service_->RequestAllocateRegion(&ctx_, &request_, &responder_, cq_, cq_, this);
    };
    void handle() override
    {
      (new AllocateRegionHandler(service_, cq_))->Proceed();

      reply_.set_addr(96);
      reply_.set_length(request_.size());
      status_ = FINISH;
      // responder_.Finish(reply_, Status(grpc::StatusCode::ABORTED, "error message here"), this);
      responder_.Finish(reply_, Status::OK, this);
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

  void HandleRpcs()
  {
    // Spawn a new CallData instance to serve new clients.
    (new AllocateRegionHandler(&service_, cq_.get()))->Proceed();
    (new PingHandler(&service_, cq_.get()))->Proceed();
    void *tag;// uniquely identifies a request.
    bool ok;
    while (true) {
      GPR_ASSERT(cq_->Next(&tag, &ok));
      GPR_ASSERT(ok);
      static_cast<async_state_machine *>(tag)->Proceed();
    }
  }

  std::unique_ptr<ServerCompletionQueue> cq_;
  FAMController::AsyncService service_;
  std::unique_ptr<Server> server_;
};
}// namespace

int main(int, char **)
{
  ServerImpl server;
  server.Run();

  return 0;
}
