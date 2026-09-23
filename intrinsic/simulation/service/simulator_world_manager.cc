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

#include "intrinsic/simulation/service/simulator_world_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/cleanup/cleanup.h"
#include "absl/functional/function_ref.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/empty.pb.h"
#include "grpcpp/client_context.h"
#include "grpcpp/server_context.h"
#include "intrinsic/util/grpc/grpc.h"
#include "intrinsic/util/status/status_conversion_grpc.h"
#include "intrinsic/util/status/status_macros.h"
#include "intrinsic/world/objects/object_world_client.h"
#include "intrinsic/world/proto/object_world_service.pb.h"

namespace intrinsic {
namespace simulation {

namespace {

// Number of attempts made for a world service RPC that reports UNAVAILABLE.
constexpr int kNumWorldServiceRpcAttempts = 3;

// Creates the client context for one attempt of a world service RPC.
std::unique_ptr<grpc::ClientContext> absl_nonnull MakeWorldServiceContext(
    const grpc::ServerContext* absl_nullable server_context) {
  if (server_context == nullptr) {
    auto ctx = std::make_unique<grpc::ClientContext>();
    ConfigureClientContext(ctx.get());
    return ctx;
  }

  std::unique_ptr<grpc::ClientContext> ctx =
      grpc::ClientContext::FromServerContext(*server_context);
  const auto propagated_deadline = ctx->deadline();
  ConfigureClientContext(ctx.get());
  if (propagated_deadline < ctx->deadline()) {
    ctx->set_deadline(propagated_deadline);
  }
  return ctx;
}

// Runs `rpc` and retries it while it reports UNAVAILABLE.
absl::Status CallWorldServiceWithRetries(
    std::string_view rpc_name,
    const grpc::ServerContext* absl_nullable server_context,
    absl::FunctionRef<absl::Status(grpc::ClientContext&)> rpc) {
  absl::Status status;
  for (int attempt = 1; attempt <= kNumWorldServiceRpcAttempts; ++attempt) {
    std::unique_ptr<grpc::ClientContext> ctx =
        MakeWorldServiceContext(server_context);
    status = rpc(*ctx);
    if (!absl::IsUnavailable(status)) {
      return status;
    }

    LOG(WARNING) << "World service RPC " << rpc_name << " failed [" << status
                 << "] Attempt (" << attempt << "/"
                 << kNumWorldServiceRpcAttempts << ")";

    if (server_context != nullptr &&
        (server_context->IsCancelled() ||
         absl::FromChrono(server_context->deadline()) <= absl::Now())) {
      LOG(WARNING) << "Not retrying " << rpc_name
                   << " because the calling RPC was cancelled or its deadline "
                      "has passed.";
      return status;
    }
  }
  return status;
}

// Returns the id of the cloned world.
absl::StatusOr<std::string> CloneWorldWithContext(
    intrinsic_proto::world::ObjectWorldService::StubInterface& stub,
    const grpc::ServerContext* absl_nullable server_context,
    std::string_view world_id) {
  intrinsic_proto::world::CloneWorldRequest request;
  request.set_world_id(world_id);

  intrinsic_proto::world::WorldMetadata response;
  INTR_RETURN_IF_ERROR(CallWorldServiceWithRetries(
      "ObjectWorldService.CloneWorld", server_context,
      [&](grpc::ClientContext& ctx) {
        return ToAbslStatus(stub.CloneWorld(&ctx, request, &response));
      }));
  return response.id();
}

absl::Status DeleteWorld(
    intrinsic_proto::world::ObjectWorldService::StubInterface& stub,
    std::string_view world_id) {
  intrinsic_proto::world::DeleteWorldRequest request;
  request.set_world_id(world_id);
  google::protobuf::Empty response;

  return CallWorldServiceWithRetries(
      "ObjectWorldService.DeleteWorld", /*server_context=*/nullptr,
      [&](grpc::ClientContext& ctx) {
        return ToAbslStatus(stub.DeleteWorld(&ctx, request, &response));
      });
}

absl::Status CloneWorldToWithContext(
    intrinsic_proto::world::ObjectWorldService::StubInterface& stub,
    const grpc::ServerContext* absl_nullable server_context,
    std::string_view world_id, std::string_view dest_world_id) {
  intrinsic_proto::world::CloneWorldRequest request;
  request.set_world_id(world_id);
  request.set_cloned_world_id(dest_world_id);
  request.set_allow_overwrite(true);

  intrinsic_proto::world::WorldMetadata response;
  return CallWorldServiceWithRetries(
      "ObjectWorldService.CloneWorld", server_context,
      [&](grpc::ClientContext& ctx) {
        return ToAbslStatus(stub.CloneWorld(&ctx, request, &response));
      });
}

absl::StatusOr<std::vector<world::WorldObject>> ListSimDisabledObjects(
    std::shared_ptr<intrinsic_proto::world::ObjectWorldService::StubInterface>
        object_world_service,
    std::string_view world_id) {
  world::ObjectWorldClient world_client(world_id, object_world_service);
  INTR_ASSIGN_OR_RETURN(std::vector<world::WorldObject> objects,
                        world_client.ListObjects());
  std::vector<world::WorldObject> result;
  for (auto& object : objects) {
    if (auto sim_component = object.GetSimulationComponent();
        sim_component.has_value() && *sim_component != nullptr &&
        (*sim_component)->disabled()) {
      result.push_back(std::move(object));
    }
  }
  return result;
}

absl::Status DeleteObjectsWithServerContext(
    std::shared_ptr<intrinsic_proto::world::ObjectWorldService::StubInterface>
        object_world_service,
    const grpc::ServerContext* absl_nullable context, std::string_view world_id,
    absl::Span<const world::WorldObject> objects_to_delete) {
  world::ObjectWorldClient world_client(world_id, object_world_service);
  std::string error_message;
  absl::Status final_status = absl::OkStatus();
  for (const auto& object_to_delete : objects_to_delete) {
    if (context != nullptr && context->IsCancelled()) {
      return absl::CancelledError(
          "Server context was cancelled while deleting objects.");
    }

    auto delete_status = world_client.DeleteObject(
        object_to_delete,
        world::ObjectWorldClient::DeleteObjectWithChildrenOption::
            kReparentChildren);
    if (!delete_status.ok()) {
      if (final_status.ok()) {
        final_status = delete_status;
      }
      absl::StrAppend(&error_message, "Failed to delete '",
                      object_to_delete.Name(), "': ", delete_status.message(),
                      "\n");
    }
  }
  if (!final_status.ok()) {
    return absl::Status(final_status.code(), error_message);
  }
  return absl::OkStatus();
}

absl::Status CloneSimWorldWithServerContext(
    std::shared_ptr<intrinsic_proto::world::ObjectWorldService::StubInterface>
        object_world_service,
    const grpc::ServerContext* absl_nullable context,
    std::string_view start_world_id, std::string_view sim_world_id,
    bool ignore_disabled_object_deletion_error) {
  LOG(INFO) << "Cloning sim world [" << sim_world_id << "] from start world ["
            << start_world_id << "].";

  INTR_ASSIGN_OR_RETURN(
      std::vector<world::WorldObject> start_world_sim_disabled_objects,
      ListSimDisabledObjects(object_world_service, start_world_id));

  if (start_world_sim_disabled_objects.empty()) {
    return CloneWorldToWithContext(*object_world_service, context,
                                   start_world_id, sim_world_id);
  }

  LOG(INFO) << "Deleting " << start_world_sim_disabled_objects.size()
            << " objects from start world";

  INTR_ASSIGN_OR_RETURN(
      std::string staging_world_id,
      CloneWorldWithContext(*object_world_service, context, start_world_id));
  absl::Cleanup delete_staging_world = [object_world_service,
                                        staging_world_id]() {
    if (auto status = DeleteWorld(*object_world_service, staging_world_id);
        !status.ok()) {
      LOG(WARNING) << "Failed to delete staging world [" << staging_world_id
                   << "]: " << status;
    }
  };

  auto deletion_status = DeleteObjectsWithServerContext(
      object_world_service, context, staging_world_id,
      start_world_sim_disabled_objects);

  if (!deletion_status.ok() && !ignore_disabled_object_deletion_error) {
    return deletion_status;
  }

  LOG_IF(WARNING, !deletion_status.ok())
      << "Failed to delete some sim-disabled objects in the starting world. "
         "These objects will show up in the sim world. Error: "
      << deletion_status;
  return CloneWorldToWithContext(*object_world_service, context,
                                 staging_world_id, sim_world_id);
}

absl::Status CloneSimWorld(
    std::shared_ptr<
        intrinsic_proto::world::ObjectWorldService::StubInterface> absl_nonnull
    object_world_service,
    std::string_view start_world_id, std::string_view sim_world_id,
    bool ignore_disabled_object_deletion_error) {
  return CloneSimWorldWithServerContext(
      std::move(object_world_service), /*context=*/nullptr, start_world_id,
      sim_world_id, ignore_disabled_object_deletion_error);
}

}  // namespace

absl::StatusOr<std::unique_ptr<SimulatorWorldManager>>
SimulatorWorldManager::Create(
    std::shared_ptr<intrinsic_proto::world::ObjectWorldService::StubInterface>
        object_world_service,
    std::shared_ptr<intrinsic_proto::world::WorldUpdater::StubInterface>
        world_updater_service,
    const CreateOptions& options) {
  if (!options.start_world_id.empty()) {
    INTR_RETURN_IF_ERROR(
        CloneSimWorld(object_world_service, options.start_world_id,
                      options.simulator_world_id,
                      options.ignore_disabled_object_deletion_error));
  } else {
    INTR_ASSIGN_OR_RETURN(
        bool exists,
        CheckWorldExists(*object_world_service, options.simulator_world_id));
    if (!exists) {
      return absl::NotFoundError(absl::StrCat(
          "Simulator world ", options.simulator_world_id, " does not exist."));
    }
  }

  auto manager = absl::WrapUnique(new SimulatorWorldManager(
      std::move(object_world_service), std::move(world_updater_service),
      options.simulator_world_id));

  manager->UpdateGenerationId();
  return manager;
}

SimulatorWorldManager::SimulatorWorldManager(
    std::shared_ptr<intrinsic_proto::world::ObjectWorldService::StubInterface>
        object_world_service,
    std::shared_ptr<intrinsic_proto::world::WorldUpdater::StubInterface>
        world_updater_service,
    std::string_view simulator_world_id)
    : object_world_service_(std::move(object_world_service)),
      world_updater_service_(std::move(world_updater_service)),
      simulator_world_id_(simulator_world_id) {}

SimulatorWorldManager::~SimulatorWorldManager() = default;

absl::Status SimulatorWorldManager::StopUpdatesAndResetWorld(
    const grpc::ServerContext* context, std::string_view start_world_id,
    bool ignore_disabled_object_deletion_error) {
  if (world_updater_service_ != nullptr) {
    INTR_RETURN_IF_ERROR(CallWorldServiceWithRetries(
        "WorldUpdater.Pause", context, [this](grpc::ClientContext& ctx) {
          intrinsic_proto::world::PauseUpdaterRequest req;
          intrinsic_proto::world::PauseUpdaterResponse resp;
          return ToAbslStatus(world_updater_service_->Pause(&ctx, req, &resp));
        }));
  }

  // Reset the simulator world by cloning it from the start world.
  INTR_RETURN_IF_ERROR(CloneSimWorldWithServerContext(
      object_world_service_, context, start_world_id, simulator_world_id_,
      ignore_disabled_object_deletion_error));

  UpdateGenerationId();
  return absl::OkStatus();
}

absl::Status SimulatorWorldManager::RestartUpdates() {
  if (world_updater_service_ != nullptr) {
    // No server context is passed on purpose: updates have to be restarted
    // even if the call that stopped them was cancelled or timed out.
    INTR_RETURN_IF_ERROR(CallWorldServiceWithRetries(
        "WorldUpdater.Resume", /*server_context=*/nullptr,
        [this](grpc::ClientContext& ctx) {
          intrinsic_proto::world::ResumeUpdaterRequest req;
          intrinsic_proto::world::ResumeUpdaterResponse resp;
          return ToAbslStatus(world_updater_service_->Resume(&ctx, req, &resp));
        }));
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> SimulatorWorldManager::CheckWorldExists(
    intrinsic_proto::world::ObjectWorldService::StubInterface& stub,
    std::string_view world_id) {
  intrinsic_proto::world::GetWorldRequest request;
  request.set_world_id(world_id);
  intrinsic_proto::world::World response;
  absl::Status status = CallWorldServiceWithRetries(
      "ObjectWorldService.GetWorld", /*server_context=*/nullptr,
      [&](grpc::ClientContext& ctx) {
        return ToAbslStatus(stub.GetWorld(&ctx, request, &response));
      });
  if (status.ok()) {
    return true;
  }
  if (absl::IsNotFound(status)) {
    return false;
  }
  return status;
}

std::string SimulatorWorldManager::generation_id() const {
  absl::MutexLock lock(&generation_mutex_);
  return generation_id_;
}

void SimulatorWorldManager::UpdateGenerationId() {
  absl::MutexLock lock(&generation_mutex_);
  generation_id_ =
      absl::StrCat(simulator_world_id_, "_", absl::ToUnixMicros(absl::Now()));
}

}  // namespace simulation
}  // namespace intrinsic
