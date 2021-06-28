CXX = g++
CXXFLAGS += -std=c++17
LDFLAGS += -L /usr/local/lib -lgrpc++_reflection -lgrpc++ -lprotobuf

PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTOS_PATH = ../protos

vpath %.proto $(PROTOS_PATH)

all: dummy_client dummy_server

dummy_client: echo.pb.cc echo.grpc.pb.cc dummy_client.cc
	$(CXX) $^ $(LDFLAGS) -o $@

dummy_server: echo.pb.cc echo.grpc.pb.cc dummy_server.cc
	$(CXX) $^ $(LDFLAGS) -o $@

.PRECIOUS: %.grpc.pb.cc
%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=. \
		--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h dummy_client dummy_server
