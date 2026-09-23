# Copyright 2026 Intrinsic Innovation LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("//bazel:python.bzl", "py_library", "py_test")

package(default_visibility = [
    "//visibility:public",
])

py_library(
    name = "proto_conversion",
    srcs = ["proto_conversion.py"],
    deps = [
        ":data_types",
        "@ai_intrinsic_sdks_pip_deps//numpy",
        "@intrinsic_apis//intrinsic/icon/proto:cart_space_py_pb2",
        "@intrinsic_apis//intrinsic/math/proto:array_py_pb2",
        "@intrinsic_apis//intrinsic/math/proto:matrix_py_pb2",
        "@intrinsic_apis//intrinsic/math/proto:point_py_pb2",
        "@intrinsic_apis//intrinsic/math/proto:pose_py_pb2",
        "@intrinsic_apis//intrinsic/math/proto:quaternion_py_pb2",
        "@intrinsic_apis//intrinsic/math/proto:twist_py_pb2",
        "@intrinsic_apis//intrinsic/math/proto:vector3_py_pb2",
    ],
)

py_library(
    name = "ros_proto_conversion",
    srcs = ["ros_proto_conversion.py"],
    deps = [
        ":data_types",
        "//third_party/ros2/ros_interfaces/jazzy/geometry_msgs/msg:point_py_pb2",
        "//third_party/ros2/ros_interfaces/jazzy/geometry_msgs/msg:pose_py_pb2",
        "//third_party/ros2/ros_interfaces/jazzy/geometry_msgs/msg:quaternion_py_pb2",
        "//third_party/ros2/ros_interfaces/jazzy/geometry_msgs/msg:twist_py_pb2",
        "//third_party/ros2/ros_interfaces/jazzy/geometry_msgs/msg:vector3_py_pb2",
        "@ai_intrinsic_sdks_pip_deps//numpy",
    ],
)

py_library(
    name = "data_types",
    srcs = ["data_types.py"],
    deps = [
        ":math_types",
        ":pose3",
        ":quaternion",
        ":rotation3",
        ":vector_util",
        "@ai_intrinsic_sdks_pip_deps//numpy",
        "@intrinsic_apis//intrinsic/icon/proto:cart_space_py_pb2",
    ],
)

py_library(
    name = "math_types",
    srcs = ["math_types.py"],
    deps = ["@ai_intrinsic_sdks_pip_deps//numpy"],
)

py_test(
    name = "math_types_test",
    size = "small",
    srcs = ["math_types_test.py"],
    deps = [
        ":math_types",
        "@ai_intrinsic_sdks_pip_deps//numpy",
    ],
)

py_library(
    name = "pose3",
    srcs = [
        "pose3.py",
    ],
    deps = [
        ":math_types",
        ":quaternion",
        ":rotation3",
        ":vector_util",
        "@ai_intrinsic_sdks_pip_deps//numpy",
    ],
)

py_library(
    name = "rotation3",
    srcs = [
        "rotation3.py",
    ],
    deps = [
        ":math_types",
        ":quaternion",
        ":vector_util",
        "@ai_intrinsic_sdks_pip_deps//numpy",
    ],
)

py_library(
    name = "quaternion",
    srcs = [
        "quaternion.py",
    ],
    deps = [
        ":math_types",
        ":vector_util",
        "@ai_intrinsic_sdks_pip_deps//numpy",
    ],
)

py_library(
    name = "vector_util",
    srcs = [
        "vector_util.py",
    ],
    deps = [
        ":math_types",
        "@ai_intrinsic_sdks_pip_deps//numpy",
    ],
)
