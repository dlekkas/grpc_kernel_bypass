#include <grpcpp/grpcpp.h>
#include <grpcpp/resource_quota.h>

#include <string>
#include <iostream>
#include <memory>

#include "./echo.grpc.pb.h"

class EchoClient {
 public:
	explicit EchoClient(std::shared_ptr<grpc::Channel> channel)
			: stub_(benchmark::EchoService::NewStub(channel)) {}

	std::string Echo(const std::string& user) {
		benchmark::EchoRequest request;
		request.set_name(user);
		request.set_id(42);

		benchmark::EchoResponse reply;

		grpc::ClientContext context;
		grpc::Status status = stub_->Echo(&context, request, &reply);

		if (status.ok()) {
			return reply.response();
		} else {
			std::cout << status.error_code() << std::endl;
			return "RPC failed";
		}
	}

 private:
	std::unique_ptr<benchmark::EchoService::Stub> stub_;
};

int main(int argc, char** argv) {
	grpc::ResourceQuota rq{"SingleThreadQuota"};
	grpc::ChannelArguments args;
	args.SetResourceQuota(rq.SetMaxThreads(1));

	std::string target_str {"172.31.76.92:50051"};
	std::shared_ptr<grpc::Channel> client_channel = grpc::CreateCustomChannel(
			target_str, grpc::InsecureChannelCredentials(), args);

	EchoClient client(client_channel);

	std::string user = "Anonymous";
	int n_runs = 1;
	for (int i = 0; i < n_runs; i++) {
		std::string reply = client.Echo(user);
	  std::cout << "Message received: " << reply << std::endl;
	}
	return 0;
}
