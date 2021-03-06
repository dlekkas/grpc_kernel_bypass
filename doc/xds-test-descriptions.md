# xDS (Load-Balancing) Interop Test Case Descriptions

Client and server use [test.proto](../src/proto/grpc/testing/test.proto).

## Server

The code for the xDS test server can be found at:
[Java](https://github.com/grpc/grpc-java/blob/master/interop-testing/src/main/java/io/grpc/testing/integration/XdsTestServer.java) (other language implementations are in progress).

Server should accept these arguments:

*   --port=PORT
    *   The port the server will run on.

## Client

The base behavior of the xDS test client is to send a constant QPS of unary
messages and record the remote-peer distribution of the responses. Further, the
client must expose an implementation of the `LoadBalancerStatsService` gRPC
service to allow the test driver to validate the load balancing behavior for a
particular test case (see below for more details).

The code for the xDS test client can be at:
[Java](https://github.com/grpc/grpc-java/blob/master/interop-testing/src/main/java/io/grpc/testing/integration/XdsTestClient.java) (other language implementations are in progress).

Clients should accept these arguments:

*   --fail_on_failed_rpcs=BOOL
    *   If true, the client should exit with a non-zero return code if any RPCs
        fail after at least one RPC has succeeded, indicating a valid xDS config
        was received. This accounts for any startup-related delays in receiving
        an initial config from the load balancer. Default is false.
*   --num_channels=CHANNELS
    *   The number of channels to create to the server.
*   --qps=QPS
    *   The QPS per channel.
*   --server=HOSTNAME:PORT
    *   The server host to connect to. For example, "localhost:8080"
*   --stats_port=PORT
    *   The port for to expose the client's `LoadBalancerStatsService`
        implementation.
*   --rpc_timeout_sec=SEC
    *   The timeout to set on all outbound RPCs. Default is 20.

### XdsUpdateClientConfigureService

The xDS test client's behavior can be dynamically changed in the middle of tests.
This is achieved by invoking the `XdsUpdateClientConfigureService` gRPC service
on the test client. This can be useful for tests requiring special client behaviors
that are not desirable at test initialization and client warmup. The service is
defined as:

```
message ClientConfigureRequest {
  // Type of RPCs to send.
  enum RpcType {
    EMPTY_CALL = 0;
    UNARY_CALL = 1;
  }

  // Metadata to be attached for the given type of RPCs.
  message Metadata {
    RpcType type = 1;
    string key = 2;
    string value = 3;
  }

  // The types of RPCs the client sends.
  repeated RpcType types = 1;
  // The collection of custom metadata to be attached to RPCs sent by the client.
  repeated Metadata metadata = 2;
}

message ClientConfigureResponse {}

service XdsUpdateClientConfigureService {
  // Update the tes client's configuration.
  rpc Configure(ClientConfigureRequest) returns (ClientConfigureResponse);
}
```

The test client changes its behavior right after receiving the
`ClientConfigureRequest`. Currently it only supports configuring the type(s) 
of RPCs sent by the test client and metadata attached to each type of RPCs.

## Test Driver

Note that, unlike our other interop tests, neither the client nor the server has
any notion of which of the following test scenarios is under test. Instead, a
separate test driver is responsible for configuring the load balancer and the
server backends, running the client, and then querying the client's
`LoadBalancerStatsService` to validate load balancer behavior for each of the
tests described below.

## LoadBalancerStatsService

The service is defined as:

```
message LoadBalancerStatsRequest {
  // Request stats for the next num_rpcs sent by client.
  int32 num_rpcs = 1;
  // If num_rpcs have not completed within timeout_sec, return partial results.
  int32 timeout_sec = 2;
}

message LoadBalancerStatsResponse {
  // The number of completed RPCs for each peer.
  map<string, int32> rpcs_by_peer = 1;
  // The number of RPCs that failed to record a remote peer.
  int32 num_failures = 2;
}

message LoadBalancerAccumulatedStatsRequest {}

message LoadBalancerAccumulatedStatsResponse {
  // The total number of RPCs have ever issued for each type.
  map<string, int32> num_rpcs_started_by_method = 1;
  // The total number of RPCs have ever completed successfully for each type.
  map<string, int32> num_rpcs_succeeded_by_method = 2;
  // The total number of RPCs have ever failed for each type.
  map<string, int32> num_rpcs_failed_by_method = 3;
}

service LoadBalancerStatsService {
  // Gets the backend distribution for RPCs sent by a test client.
  rpc GetClientStats(LoadBalancerStatsRequest)
      returns (LoadBalancerStatsResponse) {}
  // Gets the accumulated stats for RPCs sent by a test client.
  rpc GetClientAccumulatedStats(LoadBalancerAccumulatedStatsRequest)
      returns (LoadBalancerAccumulatedStatsResponse) {}
}
```

Note that the `LoadBalancerStatsResponse` contains the remote peer distribution
of the next `num_rpcs` *sent* by the client after receiving the
`LoadBalancerStatsRequest`. It is important that the remote peer distribution be
recorded for a block of consecutive outgoing RPCs, to validate the intended
distribution from the load balancer, rather than just looking at the next
`num_rpcs` responses received from backends, as different backends may respond
at different rates.

## Test Cases

### ping_pong

This test verifies that every backend receives traffic.

Client parameters:

1.  --num_channels=1
1.  --qps=100
1.  --fail_on_failed_rpc=true

Load balancer configuration:

1.  4 backends are created in a single managed instance group (MIG).

Test driver asserts:

1.  All backends receive at least one RPC

### round_robin

This test verifies that RPCs are evenly routed according to an unweighted round
robin policy.

Client parameters:

1.  --num_channels=1
1.  --qps=100
1.  --fail_on_failed_rpc=true

Load balancer configuration:

1.  4 backends are created in a single MIG.

Test driver asserts that:

1.  Once all backends receive at least one RPC, the following 100 RPCs are
    evenly distributed across the 4 backends.

### backends_restart

This test verifies that the load balancer will resume sending traffic to a set
of backends that is stopped and then resumed.

Client parameters:

1.  --num_channels=1
1.  --qps=100

Load balancer configuration:

1.  4 backends are created in a single MIG.

Test driver asserts:

1.  All backends receive at least one RPC.

The test driver records the peer distribution for a subsequent block of 100 RPCs
then stops the backends.

Test driver asserts:

1.  No RPCs from the client are successful.

The test driver resumes the backends.

Test driver asserts:

1.  Once all backends receive at least one RPC, the distribution for a block of
    100 RPCs is the same as the distribution recorded prior to restart.

### secondary_locality_gets_requests_on_primary_failure

This test verifies that backends in a secondary locality receive traffic when
all backends in the primary locality fail.

Client parameters:

1.  --num_channels=1
1.  --qps=100

Load balancer configuration:

1.  The primary MIG with 2 backends in the same zone as the client
1.  The secondary MIG with 2 backends in a different zone

Test driver asserts:

1.  All backends in the primary locality receive at least 1 RPC.
1.  No backends in the secondary locality receive RPCs.

The test driver stops the backends in the primary locality.

Test driver asserts:

1.  All backends in the secondary locality receive at least 1 RPC.

The test driver resumes the backends in the primary locality.

Test driver asserts:

1.  All backends in the primary locality receive at least 1 RPC.
1.  No backends in the secondary locality receive RPCs.

### secondary_locality_gets_no_requests_on_partial_primary_failure

This test verifies that backends in a failover locality do not receive traffic
when at least one of the backends in the primary locality remain healthy.

**Note:** Future TD features may change the expected behavior and require
changes to this test case.

Client parameters:

1.  --num_channels=1
1.  --qps=100

Load balancer configuration:

1.  The primary MIG with 2 backends in the same zone as the client
1.  The secondary MIG with 2 backends in a different zone

Test driver asserts:

1.  All backends in the primary locality receive at least 1 RPC.
1.  No backends in the secondary locality receive RPCs.

The test driver stops one of the backends in the primary locality.

Test driver asserts:

1.  All backends in the primary locality receive at least 1 RPC.
1.  No backends in the secondary locality receive RPCs.

### remove_instance_group

This test verifies that a remaining instance group can successfully serve RPCs
after removal of another instance group in the same zone.

Client parameters:

1.  --num_channels=1
1.  --qps=100

Load balancer configuration:

1.  Two MIGs with two backends each, using rate balancing mode.

Test driver asserts:

1.  All backends receive at least one RPC.

The test driver removes one MIG.

Test driver asserts:

1.  All RPCs are directed to the two remaining backends (no RPC failures).

### change_backend_service

This test verifies that the backend service can be replaced and traffic routed
to the new backends.

Client parameters:

1.  --num_channels=1
1.  --qps=100
1.  --fail_on_failed_rpc=true

Load balancer configuration:

1.  One MIG with two backends

Test driver asserts:

1.  All backends receive at least one RPC.

The test driver creates a new backend service containing a MIG with two backends
and changes the TD URL map to point to this new backend service.

Test driver asserts:

1.  All RPCs are directed to the new backend service.

### traffic_splitting

This test verifies that the traffic will be distributed between backend
services with the correct weights when route action is set to weighted
backend services.

Client parameters:

1.  --num_channels=1
1.  --qps=100

Load balancer configuration:

1.  One MIG with one backend

Assert:

1. Once all backends receive at least one RPC, the following 1000 RPCs are
all sent to MIG_a.

The test driver adds a new MIG with 1 backend, and changes the route action
to weighted backend services with {a: 20, b: 80}.

Assert:

1. Once all backends receive at least one RPC, the following 1000 RPCs are
distributed across the 2 backends as a: 20, b: 80.
### path_matching

This test verifies that the traffic for a certain RPC can be routed to a
specific cluster based on the RPC path.

Client parameters:

1.  ???num_channels=1
1.  ???qps=10
1.  ???fail_on_failed_rpc=true
1.  ???rpc=???EmptyCall,UnaryCall???

Load balancer configuration:

1.  2 MIGs, each with 1 backend
1.  routes
    - ???/???: MIG_default

Assert:

1.  UnaryCall RPCs are sent to MIG_default
1.  EmptyCall RPCs are sent to MIG_default

The test driver adds a route for EmptyCall, routes become:

1.  path{???/grpc.testing.TestService/EmptyCall???}: MIG_2
1.  ???/???: MIG_default

Assert:

1.  UnaryCall RPCs are sent to MIG_default
1.  EmptyCall RPCs are sent to MIG_2

The test driver adds a route for prefix Unary, routes become:

1.  prefix{???/grpc.testing.TestService/Unary???}: MIG_2
1.  ???/???: MIG_default

Assert:

1.  UnaryCall RPCs are sent to MIG_2
1.  EmptyCall RPCs are sent to MIG_default

### header_matching

This test verifies that the traffic for a certain RPC can be routed to a
specific cluster based on the RPC header (metadata).

Client parameters:

1.  ???num_channels=1
1.  ???qps=10
1.  ???fail_on_failed_rpc=true
1.  ???rpc=???EmptyCall,UnaryCall???
1.  ???rpc=???EmptyCall:xds_md:exact_match???

Load balancer configuration:

1.  2 MIGs, each with 1 backend
1.  routes
    - ???/???: MIG_default

Assert:

1.  UnaryCall RPCs are sent to MIG_default
1.  EmptyCall RPCs are sent to MIG_default

The test driver adds a route for header exact match, routes become:

1.  header{???xds_md???, exact: ???exact_match???}: MIG_2
1.  ???/???: MIG_default

Assert:

1.  UnaryCall RPCs are sent to MIG_default
1.  EmptyCall RPCs are sent to MIG_2

### gentle_failover

This test verifies that traffic is partially diverted to a secondary locality
when > 50% of the instances in the primary locality are unhealthy.

Client parameters:

1.  --num_channels=1
1.  --qps=100

Load balancer configuration:

1.  The primary MIG with 3 backends in the same zone as the client
1.  The secondary MIG with 2 backends in a different zone

Test driver asserts:

1.  All backends in the primary locality receive at least 1 RPC.
1.  No backends in the secondary locality receive RPCs.

The test driver stops 2 of 3 backends in the primary locality.

Test driver asserts:

1.  All backends in the secondary locality receive at least 1 RPC.
1.  The remaining backend in the primary locality receives at least 1 RPC.

The test driver resumes the backends in the primary locality.

Test driver asserts:

1.  All backends in the primary locality receive at least 1 RPC.
1.  No backends in the secondary locality receive RPCs.

### circuit_breaking

This test verifies that the maximum number of outstanding requests is limited
by circuit breakers of the backend service.

Client parameters:

1.  --num_channels=1
1.  --qps=100

Load balancer configuration:

1.  Two MIGs with each having two backends.

The test driver configures the backend services with:

1. path{???/grpc.testing.TestService/UnaryCall"}: MIG_1
1. path{???/grpc.testing.TestService/EmptyCall"}: MIG_2
1. MIG_1 circuit_breakers with max_requests = 500
1. MIG_2 circuit breakers with max_requests = 1000

The test driver configures the test client to send both UnaryCall and EmptyCall,
with all RPCs keep-open.

Assert:

1.  After reaching steady state, there are 500 UnaryCall RPCs in-flight
and 1000 EmptyCall RPCs in-flight.

The test driver updates MIG_1's circuit breakers with max_request = 800.

Test driver asserts:

1.  After reaching steady state, there are 800 UnaryCall RPCs in-flight.