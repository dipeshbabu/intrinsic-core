// Copyright 2026 Intrinsic Innovation LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "intrinsic/simulation/service/simulation_service_startup.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "grpc/grpc.h"
#include "grpcpp/channel.h"
#include "grpcpp/client_context.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/support/channel_arguments.h"
#include "intrinsic/connect/cc/grpc/channel.h"
#include "intrinsic/icon/cc_client/client.h"
#include "intrinsic/platform/pubsub/pubsub.h"
#include "intrinsic/resources/client/resource_registry_client.h"
#include "intrinsic/resources/client/resource_registry_client_interface.h"
#include "intrinsic/simulation/service/proto/first_party/simulation_service.grpc.pb.h"
#include "intrinsic/simulation/service/resource_registry_utils.h"
#include "intrinsic/simulation/service/simulation_service_collection.h"
#include "intrinsic/simulation/service/simulator.h"
#include "intrinsic/simulation/service/simulator_v1.h"
#include "intrinsic/storage/hot_shared_state/proto/application_service.grpc.pb.h"
#include "intrinsic/util/grpc/channel.h"
#include "intrinsic/util/grpc/connection_params.h"
#include "intrinsic/util/grpc/grpc.h"
#include "intrinsic/util/status/ret_check.h"
#include "intrinsic/util/status/status_builder.h"
#include "intrinsic/util/status/status_conversion_grpc.h"
#include "intrinsic/util/status/status_macros.h"
#include "intrinsic/world/proto/object_world_service.grpc.pb.h"
#include "intrinsic/world/service/updater/world_updater.grpc.pb.h"

namespace intrinsic {
namespace simulation {
namespace {

// Retry policy so that rpcs are robust to transient errors (e.g., during pod
// restarts).
constexpr char kWorldServiceRetryPolicy[] = R"(
        {
          "methodConfig": [{
            "name": [
              {"service": "intrinsic_proto.world.ObjectWorldService"},
              {"service": "intrinsic_proto.world.WorldUpdater"}
            ],
            "waitForReady": true,
            "timeout": "300s",
            "retryPolicy": {
                "maxAttempts": 5,
                "initialBackoff": "1s",
                "maxBackoff": "10s",
                "backoffMultiplier": 1.5,
                "retryableStatusCodes": ["UNAVAILABLE"]
            }
          }]
        })";

::grpc::ChannelArguments WorldServiceChannelArgs() {
  ::grpc::ChannelArguments channel_args = connect::DefaultGrpcChannelArgs();
  channel_args.SetServiceConfigJSON(kWorldServiceRetryPolicy);
  return channel_args;
}

absl::StatusOr<
    std::shared_ptr<intrinsic_proto::world::ObjectWorldService::Stub>>
CreateObjectWorldServiceStub(const std::string& world_service_address,
                             absl::Duration grpc_connect_timeout) {
  INTR_ASSIGN_OR_RETURN(
      const std::shared_ptr<grpc::Channel> channel,
      connect::CreateClientChannel(world_service_address,
                                   absl::Now() + grpc_connect_timeout,
                                   WorldServiceChannelArgs()));
  return intrinsic_proto::world::ObjectWorldService::NewStub(channel);
}

absl::StatusOr<std::shared_ptr<intrinsic_proto::world::WorldUpdater::Stub>>
CreateWorldUpdaterServiceStub(const std::string& world_service_address,
                              absl::Duration grpc_connect_timeout) {
  INTR_ASSIGN_OR_RETURN(
      const std::shared_ptr<grpc::Channel> channel,
      connect::CreateClientChannel(world_service_address,
                                   absl::Now() + grpc_connect_timeout,
                                   WorldServiceChannelArgs()));
  return intrinsic_proto::world::WorldUpdater::NewStub(channel);
}

absl::StatusOr<std::unique_ptr<intrinsic_proto::hot_shared_state::v1::
                                   HotSharedStateApplicationService::Stub>>
CreateHSSApplicationServiceStub(
    const std::string& hss_application_service_address,
    absl::Duration grpc_connect_timeout) {
  INTR_ASSIGN_OR_RETURN(
      auto channel,
      connect::GrpcChannel(hss_application_service_address)
          .WithChannelCredentials(grpc::InsecureChannelCredentials())
          .WithTimeout(grpc_connect_timeout)
          .Connect());
  return intrinsic_proto::hot_shared_state::v1::
      HotSharedStateApplicationService::NewStub(channel);
}

absl::StatusOr<
    std::unique_ptr<intrinsic::resources::ResourceRegistryClientInterface>>
CreateResourceRegistryClient(const std::string& resource_registry_address,
                             absl::Duration grpc_connect_timeout) {
  return intrinsic::resources::CreateResourceRegistryClient(
      resource_registry_address, absl::Seconds(60), grpc_connect_timeout);
}

absl::StatusOr<icon::Client> CreateIconClient(const ConnectionParams& params) {
  INTR_ASSIGN_OR_RETURN(auto channel, Channel::MakeFromAddress(params));
  return icon::Client(channel);
}

absl::StatusOr<ResourceConnectionInfo>
GetSimulatorConnectionParamsFromResourceRegistry(
    const std::string& resource_registry_address,
    absl::Duration grpc_connect_timeout) {
  INTR_ASSIGN_OR_RETURN(
      std::vector<ResourceConnectionInfo> simulators,
      GetResourcesFromRegistry(
          resource_registry_address, grpc_connect_timeout,
          "intrinsic_proto.simulation.v1.SimulatorControlService"));
  INTR_RET_CHECK(!simulators.empty());
  LOG(INFO) << "Received " << simulators.size()
            << " simulator resources back from the resource registry.";

  if (simulators.size() > 1) {
    auto builder = FailedPreconditionErrorBuilder()
                   << "Multiple simulators are installed: ";
    for (size_t i = 0; i < simulators.size(); ++i) {
      if (i > 0) {
        builder << ", ";
      }
      builder << simulators[i].name;
    }
    builder << ". Only one simulator instance should be installed.";
    return builder;
  }

  return simulators[0];
}

absl::StatusOr<std::unique_ptr<Simulator>> CreateSimulator(
    const SimulationServiceStartupOptions& options) {
  if (options.simulator_from_resource_registry) {
    CHECK(!options.resource_registry_address.empty())
        << "Resource registry address cannot be empty if "
           "--simulator_from_resource_registry is set.";
    INTR_ASSIGN_OR_RETURN(
        ResourceConnectionInfo simulator_info,
        GetSimulatorConnectionParamsFromResourceRegistry(
            options.resource_registry_address, options.grpc_connect_timeout));
    return std::make_unique<SimulatorV1>(SimulatorV1::Config{
        .sim_control_connection_params =
            std::move(simulator_info.connection_params),
        .simulator_name = simulator_info.name,
    });
  }

  CHECK(!options.sim_control_address.empty())
      << "Simulator control service address cannot be empty.";
  return std::make_unique<SimulatorV1>(SimulatorV1::Config{
      .sim_control_connection_params =
          ConnectionParams{
              .address = options.sim_control_address,
          },
      .simulator_name = "Gazebo internal (deprecated)",
  });
}

}  // namespace

absl::StatusOr<std::unique_ptr<SimulationServiceCollection>>
RunSimulationService(const SimulationServiceStartupOptions& options) {
  SimulationServiceImpl::ResourceRegistryClientFactory
      create_resource_registry_client;
  if (!options.resource_registry_address.empty()) {
    create_resource_registry_client = [&options]() {
      return CreateResourceRegistryClient(options.resource_registry_address,
                                          options.grpc_connect_timeout);
    };
  }

  if (options.world_service_address.empty()) {
    return absl::InvalidArgumentError("--world_service_address must be set.");
  }
  SimulationServiceImpl::ObjectWorldServiceClientFactory
      connect_to_object_world_service = [&options]() {
        return CreateObjectWorldServiceStub(options.world_service_address,
                                            options.grpc_connect_timeout);
      };
  SimulationServiceImpl::WorldUpdaterClientFactory
      connect_to_world_updater_service = [&options]() {
        return CreateWorldUpdaterServiceStub(options.world_service_address,
                                             options.grpc_connect_timeout);
      };

  std::unique_ptr<intrinsic_proto::hot_shared_state::v1::
                      HotSharedStateApplicationService::Stub>
      hss_application_service_stub;
  if (!options.hss_application_service_address.empty()) {
    INTR_ASSIGN_OR_RETURN(
        hss_application_service_stub,
        CreateHSSApplicationServiceStub(options.hss_application_service_address,
                                        options.grpc_connect_timeout),
        _.LogError() << "Failed to connect to the hot shared state application "
                        "service at address ["
                     << options.hss_application_service_address << "]");
  }

  return SimulationServiceCollection::Create(
      [&options]() { return CreateSimulator(options); }, &CreateIconClient,
      std::move(connect_to_object_world_service),
      std::move(hss_application_service_stub),
      std::move(create_resource_registry_client),
      std::move(connect_to_world_updater_service),
      /*manual_application_layer_targets=*/{}, options.pubsub);
}

}  // namespace simulation
}  // namespace intrinsic
