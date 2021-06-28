#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include <iostream>
#include <memory>
#include <string>

#include "./echo.grpc.pb.h"

class EchoServiceImpl final : public benchmark::EchoService::Service {
  grpc::Status Echo(grpc::ServerContext* context,
                        const benchmark::EchoRequest* request,
                        benchmark::EchoResponse* reply) override {
    reply->set_response(request->name()); 
    return grpc::Status::OK;
  }
};

void RunServer() {
  std::string server_addr {"172.31.73.146:50051"};
  // std::string server_addr {"172.31.13.210:50051"};
  // std::string server_addr {"0.0.0.0:50051"};
  EchoServiceImpl service;

  grpc::EnableDefaultHealthCheckService(false);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;

  builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_addr << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}


