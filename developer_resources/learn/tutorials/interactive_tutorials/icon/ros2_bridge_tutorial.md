
# ros2_control compatibility

If you followed the [Robot
Bringup](/developer_resources/learn/tutorials/interactive_tutorials/icon/robot_bringup.md)
tutorial, you know that the Intrinsic Core (IC) contains hardware modules (HWMs)
for a handful of manufacturers. But what if your favorite robot is not supported
out of the box?

If there is a [`ros2_control`](https://control.ros.org/rolling/index.html)
driver for your robot, the easiest way to get started is to use
[`icon_hwm_controller`](https://github.com/intrinsic-ai/icon-hwm-controller) to
connect that driver to ICON, the Intrinsic realtime control service.

## `icon_hwm_controller` introduction

The diagram below shows how `icon_hwm_controller` fits into the `ros2_control`
and ICON ecosystems:

![Architecture diagram for icon_hwm_controller. The controller lives inside the
ROS2 controller_manager, but connects to ICON via shared-memory
IPC.](icon_hwm_controller_architecture.svg)

-`icon_hwm_controller` slots into the existing `ros2_control` _and_ ICON
architecture by implementing two interfaces:

* `ros2_control`'s
  [`Controller`](https://control.ros.org/rolling/doc/getting_started/getting_started.html#controllers)
  interface (like `joint_trajectory_controller` in a regular ROS2 system)
* ICON's
  [`HardwareModuleInterface`](https://github.com/intrinsic-ai/icon-shared-memory/blob/main/icon/hal/hardware_module_interface.h)

Because ICON connects to hardware modules using a shared-memory IPC mechanism,
ICON and `icon_hwm_controller` can connect to each other, even though the latter
is a plugin within the `ControllerManager` ROS2 node.

## Use `icon_hwm_controller` to connect a robot to ICON

`icon_hwm_controller` comes with some
[examples](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main/icon_hwm_controller_examples),
so if your robot is covered by those, you're in luck. If not, don't worry. It's
relatively easy to add support for any robot model that has a `ros2_control`
driver:

1. Clone https://github.com/intrinsic-ai/icon-hwm-controller and build the
   [`icon_hwm_base` docker container]():

   ```bash
   git clone --recurse-submodules --shallow-submodules https://github.com/intrinsic-ai/icon-hwm-controller.git
   cd icon-hwm-controller
   docker compose -f icon_hwm_controller/docker/docker-compose.yml build
   ```
2. Create a new folder for your driver wrapper. This folder needs a few things
   (the links go to [the UR driver
   example](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main/icon_hwm_controller_examples/ur_ros2_icon_hwm). You
   can use these for reference):

   * [`package.xml`](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main/icon_hwm_controller_examples/ur_ros2_icon_hwm/package.xml)
     and
     [`CMakeLists.txt`](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main/icon_hwm_controller_examples/ur_ros2_icon_hwm/CMakeLists.txt)
     files for a ROS2 package (you will create a launch file and, optionally, a
     ROS2 node in the following steps)
   * [`Dockerfile`](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main/icon_hwm_controller_examples/ur_ros2_icon_hwm/docker/Dockerfile)
     and
     [`docker-compose.yml`](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main/icon_hwm_controller_examples/ur_ros2_icon_hwm/docker/docker-compose.yml)
     to pull in your `ros2_control` driver and build/package them in a container
   * [`.bazelversion`](https://github.com/intrinsic-ai/icon-hwm-controller/blob/main/.bazelversion),
     [`MODULE.bazel`](https://github.com/intrinsic-ai/icon-hwm-controller/blob/main/MODULE.bazel)
     and
     [`BUILD`](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main/icon_hwm_controller_examples/ur_ros2_icon_hwm/BUILD)
     files to configure the bazel build for the final Intrinsic assets

   Note that your folder can be a separate repo from `icon-hwm-controller`. Your
   Dockerfile should depend on the
   [`icon_hwm_base`](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main/icon_hwm_controller/docker)
   image, which contains everything you need to use `icon_hwm_controller`.

3. Start with a working ROS2 launch file for your robot, then remove all
   controllers other than `joint_state_broadcaster` and
   `joint_trajectory_controller`, plus any others that you need in order to
   manage operational state (fault reporting and clearing).

   Test this launch file to make sure it works with your *real* robot.

4. [Optional] Implement an `OperationalStatus` node for your robot.

   If you are just experimenting, and are fine with restarting your driver
   process manually to recover from things like emergency stops or limit
   violations, you can skip this step. However, having nice error reporting and
   the ability to quickly get up and running after a mishap can do wonders for
   your iteration time, so building an `OperationalStatus` node is time spent
   well if you plan to use your robot with the Intrinsic platform a lot. (Or if
   you *weren't* planning to do that, but notice you're doing it anyway!)

   Since `ros2_control` does not have unified error reporting and handling, each
   robot driver needs a bespoke node that provides two things:

   * An `icon_hwm_controller_msgs/msg/OperationalStatus` publisher that tells
     `icon_hwm_controller` if there is a problem (and, with its `message` field,
     what that problem is).
   * An `std_srvs/srv/Trigger` service server that clears any problems to the
     best of its ability, and returns the robot to a "ready" state.

   Look at the example nodes for
   [UR](https://github.com/intrinsic-ai/icon-hwm-controller/blob/main/icon_hwm_controller_examples/ur_ros2_icon_hwm/src/ur_operational_state_node.cpp)
   and
   [FANUC](https://github.com/intrinsic-ai/icon-hwm-controller/blob/main/icon_hwm_controller_examples/fanuc_ros2_icon_hwm/src/fanuc_operational_state_node.cpp)
   robots to get an idea of what this can look like.

5. Configure `icon_hwm_controller`.

   Usually the configuration for `ros2_control` lives in a file called
   `controllers.yaml`. Add the configuration for `icon_hwm_controller` to that
   file. You need to modify two parts of the file:

   First, add `icon_hwm_controller` to the configuration for
   `ControllerManager`. This tells the
   [`controller_manager/spawner`](https://control.ros.org/rolling/doc/ros2_control/controller_manager/doc/userdoc.html#spawner)
   node which plugin it should load the controller from.

   ```yaml
   /**:
    ros__parameters:
      update_rate: $(var control_frequency_hz)
      joint_state_broadcaster:
        type: joint_state_broadcaster/JointStateBroadcaster

      # Add these lines:
      icon_controller:
        type: icon_hwm_controller/IconHwmController

      cpu_affinity: $(var cpu_affinity)
      lock_memory: $(var lock_memory)
      thread_priority: $(var realtime_priority_low)
   ```

   Next, add the configuration block for the controller itself (outside of the
   `/**` block from before):

   ```yaml
   /**/icon_controller:
    ros__parameters:
      # The launch file supplies the variables we use here
      name: "$(var hwm_name)"
      context_name: "$(var context_name)"
      shm_namespace: "$(var shm_namespace)"
      cpu_affinity: $(var cpu_affinity)
      lock_memory: $(var lock_memory)
      realtime_priority_low: $(var realtime_priority_low)
      realtime_priority_high: $(var realtime_priority_high)
      control_frequency_hz: $(var control_frequency_hz)
      drives_realtime_clock: $(var drives_realtime_clock)

      # You need to manually configure the values for these (see below)
      dof_names:
        - "J1"
        - "J2"
        - "J3"
        - "J4"
        - "J5"
        - "J6"
      command_interfaces:
        - position
      reference_and_state_interfaces:
        - position
        - velocity
      hardware_component_name: "$(var robot_model)"
      operational_status_topic: /operational_status
      clear_faults_trigger_service: /clear_faults
   ```

   This snippet uses substitution to let the Intrinsic platform parameterize
   your wrapped driver. Check [the definition of these
   parameters](https://github.com/intrinsic-ai/icon-hwm-controller/blob/main/icon_hwm_controller/src/icon_hwm_controller_parameters.yaml)
   for documentation on each of them. You will add launch arguments later to
   provide the variables that the configuration uses.

   There are also a few parameters that the platform will *not* provide, so you
   need to decide how to populate them:

   * `dof_names`: The `ros2_control` interface prefixes for each joint, in order
     from base to tip
   * `command_interfaces`: The names of the command interfaces for your robot
     driver. In order:

     1. position
     2. velocity (optional)

     Usually, the interface names are literally "position" and "velocity".

     Note that `icon_hwm_controller` will treat the velocity interface as a
     feedforward if present, i.e. it will write commands to both the position
     and velocity interfaces. If your driver does not support this, do not
     specify a velocity interface name.
   * `reference_and_state_interfaces`: Similar to `command_interfaces`, these
     are the names of the position and velocity _state_ interfaces for your
     robot driver.
   * `hardware_component_name`: The name of the `ros2_control`
     `HardwareComponent` for your robot driver. This must match the `name`
     attribute of your `<ros2_control>` xacro tag. If your launch file uses
     parameter substitution to set that attribute, use the same substitution
     here!
   * `operational_status_topic` and `clear_faults_trigger_service`. These should
     match the topic/service names for your `OperationalStatus` node (see
     above). If you chose to __not__ create that node, omit these two.

6. Update your launch file to

   * start your `OperationalStatus` node
   * register the arguments that `icon_hwm_controller` expects (there is a
     [helper
     function](https://github.com/intrinsic-ai/icon-hwm-controller/blob/main/icon_hwm_controller/icon_hwm_controller/launch.py#L20)
     to do so)
   * spawn `icon_hwm_controller` instead of `joint_trajectory_controller` (make
     sure to spawn it as **inactive**)
   * set `allow_substs=True` when loading the `.yaml` configuration

7. Build and export your docker container, and use it to build an Intrinsic
   service asset.

8. Convert your robot's description to an `.sdf` file, and use that to create an
   [`intrinsic_scene_object`](/intrinsic/assets/scene_objects/build_defs/scene_object.bzl#L115). There
   are detailed instructions for this in the [Convert 3D models into
   SDF](../../../../assets/create_new_assets/convert_3d_models.md)
   tutorial, but you can shortcut some of the steps outlined there by converting
   from URDF to SDF.

9. Combine the service and scene object assets into a single
   [`intrinsic_hardware_device`](/intrinsic/assets/hardware_devices/build_defs/hardware_device.bzl#L24). For
   an example hardware device rule, look at
   [`kr6_r900_2_fake_hardware_module`](/intrinsic_control/intrinsic/icon/hardware_modules/kuka_rsi/BUILD#L306)

10. Sideload your `HardwareDevice` into your Intrinsic solution using `inctl`.

Well, I did say *relatively* easy. Ten steps is nothing to sneeze at, and some
of these involve writing new code (although most of it is launch files, build
configuration and container definitions). Read on for a detailed walkthrough of
the process with a simple example robot.

### RRBot walkthrough

> [!CAUTION]
> Using RRBot for now, because it's very simple. If it turns out that ICON is
> unhappy with just two joints, we can switch to
> [r6bot](https://control.ros.org/master/doc/ros2_control_demos/example_7/doc/userdoc.html)

Let's walk through all steps using the
[`RRBot`](https://control.ros.org/master/doc/ros2_control_demos/example_1/doc/userdoc.html)
example that `ros2_control` uses. This is a very simple virtual robot that
doesn't require any external hardware or simulators, so there are not many
dependencies.


#### Workspace setup

First, ensure that you have [the tools you will
need](https://github.com/intrinsic-ai/icon-hwm-controller/tree/main#prerequisites). Starting
from a regular Ubuntu installation, you will need to acquire

* [**Git**](https://git-scm.com/install/linux) (`sudo apt install git`) to clone
  the `icon_hwm_controller` repo.
* [**Docker Engine & Docker Compose**](https://docs.docker.com/engine/install/)
  to build and containerize the ROS 2 driver environments.
* [**Bazelisk / Bazel**](https://bazel.build/install/bazelisk) to build the
  Intrinsic Service and Hardware Device assets.
* **`inctl`** (Intrinsic's CLI tool) to install and manage assets on the
  Intrinsic cluster. You can use
  [`env.sh`](/developer_resources/learn/tutorials/interactive_tutorials/icon/files/env.sh)
  to create a convenient alias to use inside your checkout of the Intrinsic Core
  repo.

*(Alternatively, use the [Intrinsic
DevContainer](https://flowstate.intrinsic.ai/docs/guides/build_with_code/set_up_your_development_environment/local_environment/). The
setup instructions include Docker, and the container itself includes Bazel and
`inctl`.)*

Now, clone https://github.com/intrinsic-ai/icon-hwm-controller, including its
submodules:

```bash
git clone --recurse-submodules --shallow-submodules https://github.com/intrinsic-ai/icon-hwm-controller.git
```

For a quick sanity check, make sure that the `icon_hwm_base` container builds to
start with (this can take a while):

```bash
cd icon-hwm-controller
docker compose -f icon_hwm_controller/docker/docker-compose.yml build
```

Once this succeeds, `icon_hwm_base` is available in your local Docker registry,
and you'll be able to depend on it in your own Dockerfile.

> [!NOTE]
> Do not attempt to build the examples using `bazel`. That will **not** work at
> this point, because they rely on docker images that are too big to commit to
> Git, and impossible to build cleanly with Bazel. If you want to run one of the
> examples, follow the steps in its `README.md` file.

#### Create a folder for your robot

Simple enough:

```bash
mkdir rrbot_ros2_icon_hwm
cd rrbot_ros2_icon_hwm

# You don't have to use this exact folder structure, but it's a good start.
mkdir -p config docker include/rrbot_ros2_icon_hwm launch proto src test

touch BUILD CMakeLists.txt package.xml

cp ../ur_ros2_icon_hwm/.dockerignore .
```

#### Set up the build configuration

Throughout this walkthrough, expand the sections for each file name to see its
full content.

<details> <summary><strong>package.xml</strong></summary>

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>rrbot_ros2_icon_hwm</name>
  <version>0.1.0</version>
  <description>
    Configuration files for an ICON hardware module based on the RRBot ros2_control example.
  </description>
  <maintainer email="your.email@goes.here">Your Name</maintainer>

  <!-- Of course, feel free to use whatever license fits your use case -->
  <license>Apache-2.0</license>

  <buildtool_depend>ament_cmake</buildtool_depend>

  <!-- build-time dependencies for the OperationalStatus node -->
  <depend>icon_hwm_controller_msgs</depend>
  <depend>icon_shared_memory_vendor</depend>

  <!-- standard ROS2 deps -->
  <depend>rclcpp</depend>
  <depend>std_msgs</depend>
  <depend>std_srvs</depend>

  <!-- RRBot is defined in this package. Most real robots require several packages -->
  <depend>ros2_control_demo_example_1</depend>

  <!-- execution-time dependency for icon_hwm_controller itself -->
  <exec_depend>icon_hwm_controller</exec_depend>

  <!-- other dependencies for the launch file -->
  <exec_depend>joint_state_broadcaster</exec_depend>
  <exec_depend>controller_manager</exec_depend>
  <exec_depend>ros2_control_demo_description</exec_depend>
  <exec_depend>ros2launch</exec_depend>
  <exec_depend>xacro</exec_depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```
</details>

<details> <summary><strong>CMakeLists.txt</strong></summary>

```cmake cmake_minimum_required(VERSION 3.8) project(rrbot_ros2_icon_hwm)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Default to C++20 standard.
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(icon_hwm_controller_msgs REQUIRED)
find_package(icon_shared_memory_vendor REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(ros2_control_demo_example_1 REQUIRED)

add_executable(
  rrbot_operational_state_node
  src/rrbot_operational_state_node.cpp
)
target_include_directories(rrbot_operational_state_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(rrbot_operational_state_node PUBLIC
  ${icon_hwm_controller_msgs_TARGETS}
  icon_shared_memory_icon_utils_mutex
  icon_shared_memory_icon_utils_status
  rclcpp::rclcpp
  rclcpp_action::rclcpp_action
  ${std_msgs_TARGETS}
  ${std_srvs_TARGETS}
)

install(TARGETS
  rrbot_operational_state_node
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
```
</details>

<details> <summary><strong>Dockerfile</strong></summary>

```dockerfile FROM icon_hwm_base:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y \
    git \
    python3-colcon-common-extensions \
    python3-rosdep \
 && rm -rf /var/lib/apt/lists/*

COPY . ${AMENT_WORKSPACE_DIR}/src/rrbot_ros2_icon_hwm

RUN source /opt/ros/${ROS_DISTRO}/setup.bash \
 && git clone --depth 1 https://github.com/ros-controls/ros2_control_demos.git \
    ${AMENT_WORKSPACE_DIR}/src/ros2_control_demos \
 && git clone --depth 1 https://github.com/pal-robotics/urdf_test.git \
    ${AMENT_WORKSPACE_DIR}/src/urdf_test \
 && apt-get update \
 && rosdep update

RUN source /opt/ros/${ROS_DISTRO}/setup.bash \
 && rosdep install -iy \
    --from-path $(colcon list --packages-up-to rrbot_ros2_icon_hwm --paths-only) \
    --ignore-src \
    --rosdistro ${ROS_DISTRO} \
 && colcon build --base-paths ${AMENT_WORKSPACE_DIR} \
    --packages-up-to rrbot_ros2_icon_hwm \
 && rm -rf /var/lib/apt/lists/*

ENV DEBIAN_FRONTEND=dialog
```
</details>

<details> <summary><strong>docker-compose.yml</strong></summary>

```yaml
services:
  rrbot_ros2_icon_hwm:
    build:
      context: ..
      dockerfile: docker/Dockerfile
    container_name: rrbot_ros2_icon_hwm
    image: rrbot_ros2_icon_hwm:latest
    tty: true
    entrypoint: ["/bin/bash", "-c", "source ./install/setup.bash && exec \"$@\"", "--"]
    command: >
      colcon test --packages-select rrbot_ros2_icon_hwm
        --event-handlers console_direct+ &&
      colcon test-result --all
```
</details>

With these files set up, we can almost test the container build (we will
populate `BUILD` later). Add some dummy code to
`src/rrbot_operational_state_node.cpp` to make sure it compiles:

```c++
int main(int argc, char ** argv)
{
  return 0;
}
```

Now build the container, and run it (this runs the automatic linter tests from
`CMakeLists.txt`):

```bash
docker compose -f docker/docker-compose.yml run --build --rm --remove-orphans \
    rrbot_ros2_icon_hwm
```

#### Add a minimal launch file

[`rrbot.launch.py`](https://github.com/ros-controls/ros2_control_demos/blob/master/example_1/bringup/launch/rrbot.launch.py)
is already pretty bare-bones.  There's not much to trim here, but we won't need
RViz.

After removing RViz, your launch file should look like this:

<details> <summary><strong>launch/rrbot.launch.py</strong></summary>

```python
# Copyright 2021 Stogl Robotics Consulting UG (haftungsbeschränkt)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import Command, LaunchConfiguration, PathSubstitution

from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterFile
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription(
        [
            DeclareLaunchArgument(
                'joint_prefix',
                default_value='joint',
                description='Prefix for joint names (used with allow_substs in spawner).',
            ),
            # Control node
            Node(
                package='controller_manager',
                executable='ros2_control_node',
                parameters=[{'update_rate': 10}],
                output='both',
            ),
            # robot_state_publisher with robot_description from xacro
            Node(
                package='robot_state_publisher',
                executable='robot_state_publisher',
                output='both',
                parameters=[
                    {
                        'robot_description': Command(
                            [
                                'xacro',
                                ' ',
                                PathSubstitution(FindPackageShare('ros2_control_demo_example_1'))
                                / 'urdf'
                                / 'rrbot.urdf.xacro',
                            ]
                        )
                    }
                ],
            ),
            Node(
                package='controller_manager',
                executable='spawner',
                arguments=[
                    'joint_state_broadcaster',
                    '--param-file',
                    PathSubstitution(FindPackageShare('ros2_control_demo_example_1'))
                    / 'config'
                    / 'rrbot_controllers.yaml',
                ],
            ),
            Node(
                package='controller_manager',
                executable='spawner',
                parameters=[
                    {'joint_prefix': LaunchConfiguration('joint_prefix')},
                    ParameterFile(
                        PathSubstitution(FindPackageShare('ros2_control_demo_example_1'))
                        / 'config'
                        / 'rrbot_controllers.yaml',
                        allow_substs=True,
                    ),
                ],
                arguments=[
                    'forward_position_controller',
                ],
            ),
        ]
    )
```
</details>

Make sure the launch file starts without errors in your container:

```bash
docker compose -f docker/docker-compose.yml run --build --rm --remove-orphans \
    rrbot_ros2_icon_hwm \
    ros2 launch rrbot_ros2_icon_hwm rrbot.launch.py
```


#### `OperationalStatus` node

`RRBot` doesn't actually report any status beyond the managed node lifecycle
state, so this `OperationalStatus` node is a dummy implementation that you can
build on for your own hardware:

<details>
<summary><strong>src/rrbot_operational_status_node.cpp</strong></summary>

```c++
#include <chrono>
#include <memory>

#include "icon_hwm_controller_msgs/msg/operational_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_srvs/srv/trigger.hpp"

using namespace std::chrono_literals;

void clear_faults(
  const std::shared_ptr<std_srvs::srv::Trigger::Request>/*unused*/,
  std::shared_ptr<std_srvs::srv::Trigger::Response> response)
{
  RCLCPP_INFO(rclcpp::get_logger("rclcpp"),
              "Received ClearFaults request, sending back a successful response");
  response->success = true;
}

void publish_happy_status(
  rclcpp::Publisher<icon_hwm_controller_msgs::msg::OperationalStatus>::SharedPtr publisher)
{
  icon_hwm_controller_msgs::msg::OperationalStatus operational_status;
  operational_status.state = icon_hwm_controller_msgs::msg::OperationalStatus::ENABLED;
  // The `message` field can be empty for `ENABLED`, but should hold a helpful
  // error message in other states.
  operational_status.message = "";
  publisher->publish(operational_status);
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  std::shared_ptr<rclcpp::Node> node = rclcpp::Node::make_shared("rrbot_operational_status");

  // Create ClearFaults service
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr service =
    node->create_service<std_srvs::srv::Trigger>("clear_faults", &clear_faults);

  // Create OperationalStatus publisher and timer
  auto publisher = node->create_publisher<icon_hwm_controller_msgs::msg::OperationalStatus>(
    "operational_status", 10);
  auto timer = node->create_wall_timer(
    500ms, [&publisher](){
      publish_happy_status(publisher);
    });

  rclcpp::spin(node);
  rclcpp::shutdown();

  return 0;
}
```
</details>

#### `icon_hwm_controller` configuration

Next, we have to make a copy of [`rrbot_controllers.yaml` from the
`ros2_control`
examples](https://github.com/ros-controls/ros2_control_demos/blob/master/example_1/bringup/config/rrbot_controllers.yaml),
and add configuration value for `icon_hwm_controller`:

<details> <summary><strong>config/rrbot_controllers.yaml</strong></summary>

```yaml
joint_state_broadcaster:
  ros__parameters:
    type: joint_state_broadcaster/JointStateBroadcaster
    update_rate: 50  # Hz
    
forward_position_controller:
  ros__parameters:
    type: forward_command_controller/ForwardCommandController
    joints:
      - $(var joint_prefix)1
      - $(var joint_prefix)2
    interface_name: position

icon_hwm_controller:
  ros__parameters:
    type: icon_hwm_controller/IconHwmController
    # The launch file supplies the variables we use here
    name: "$(var hwm_name)"
    context_name: "$(var context_name)"
    shm_namespace: "$(var shm_namespace)"
    cpu_affinity: $(var cpu_affinity)
    lock_memory: $(var lock_memory)
    realtime_priority_low: $(var realtime_priority_low)
    realtime_priority_high: $(var realtime_priority_high)
    control_frequency_hz: $(var control_frequency_hz)
    drives_realtime_clock: $(var drives_realtime_clock)

    # RRBot uses a launch argument to generate joint names!
    dof_names:
      - $(var joint_prefix)1
      - $(var joint_prefix)2
    command_interfaces:
      - position
    reference_and_state_interfaces:
      # RRBot only provides a position state interface
      - position # This name is hard-coded in #
    https://github.com/ros-controls/ros2_control_demos/blob/master/example_1/description/urdf/rrbot.urdf.xacro#L27
    hardware_component_name: "RRBot" operational_status_topic:
    /operational_status clear_faults_trigger_service: /clear_faults 
```

</details>

#### Update the launch file

We want to update our launch file to do a few things:

1. Add the new launch arguments that `rrbot_controllers.yaml` now uses.

   To do that, use the `get_icon_hwm_launch_arguments()` helper:
   
   ```python
   # At the top of the launch file
   from icon_hwm_controller.launch import get_icon_hwm_launch_arguments   

   # ...

   def generate_launch_description():
       return LaunchDescription(
           # Prepend the launch arguments to the other launch description items.
           get_icon_hwm_launch_arguments() +
           [ ...
   ```
   
2. Spawn `icon_hwm_controller` instead of `forward_position_controller`, and use
   the modified `rrbot_controllers.yaml` to provide parameters:
   
   ```python
   # This replaces the spawner that launched `forward_position_controller`.
   Node(
       package='controller_manager',
       executable='spawner',
       parameters=[
           {'joint_prefix': LaunchConfiguration('joint_prefix')},
           ParameterFile(
               PathSubstitution(FindPackageShare('rrbot_ros2_icon_hwm'))
               / 'config'
               / 'rrbot_controllers.yaml',
               allow_substs=True,
           ),
       ],
       arguments=[
           '--inactive',
           'icon_hwm_controller',
       ],
   ),
   ```

3. Start the OperationalStatus node

   ```python
   Node(
       package='rrbot_ros2_icon_hwm',
       executable='rrbot_operational_state_node',
       output='screen',
   ),
   ```

With these changes, rebuild and run your container again:

```bash
docker compose -f docker/docker-compose.yml run --build --rm --remove-orphans \
    rrbot_ros2_icon_hwm \
    ros2 launch rrbot_ros2_icon_hwm rrbot.launch.py hwm_name:="icon_hwm"
```

> [!NOTE]
> The new `hwm_name` parameter is important! Without it, the launch file will
> not start.


Your launch file should start without errors, and you should see log messages
like these (note that the socket path includes the `hwm_name` you specified when
you started the launch file):

```
[ros2_control_node-1] [INFO] [1788973704.630322092] [controller_manager]: Loading controller : 'icon_hwm_controller' of type 'icon_hwm_controller/IconHwmController'
[ros2_control_node-1] [INFO] [1788973704.630430432] [controller_manager]: Loading controller 'icon_hwm_controller'
[ros2_control_node-1] [INFO] [1788973704.638885842] [controller_manager]: Controller 'icon_hwm_controller' node arguments: --ros-args --params-file /tmp/launch_params_ay22624n --params-file /tmp/launch_params_nduyr2eq --params-file /tmp/launch_params_56e8orby 
[spawner-4] [INFO] [1788973704.696788409] [spawner_icon_hwm_controller]: Loaded icon_hwm_controller
[ros2_control_node-1] [INFO] [1788973704.699077199] [controller_manager]: Configuring controller: 'icon_hwm_controller'
[ros2_control_node-1] [INFO] [1788973704.708156458] [icon_hwm_controller]: Acquired exclusive lock '/tmp/intrinsic_icon/icon_hwm.lock'.
[ros2_control_node-1] [INFO] [1788973704.710050708] [icon_hwm_controller]: Preparing message 1 of 1
[ros2_control_node-1] [INFO] [1788973704.710122918] [icon_hwm_controller]: Message 1 of 1 has 27 FDs
[ros2_control_node-1] [INFO] [1788973704.710189478] [icon_hwm_controller]: Socket '/tmp/intrinsic_icon/icon_hwm.sock' is serving 27 descriptors.
[ros2_control_node-1] [INFO] [1788973704.710405408] [icon_hwm_controller]: Waiting for new connection
```

#### Build an Intrinsic service asset from your ROS2 container

To package the `rrbot_ros2_icon_hwm` docker container into an Intrinsic service
asset, first create and populate `.bazelversion`, `MODULE.bazel` and `BUILD`
files at the root of your directory:

**.bazelversion**

```
8.8.0
```

<details> <summary><strong>MODULE.bazel</strong></summary>

```bazel
module(name = "rrbot_ros2_hwm_asset")

bazel_dep(name = "ai_intrinsic_sdks")
# Download the Intrinsic SDKs from Github.
archive_override(
    module_name = "ai_intrinsic_sdks",
    strip_prefix = "sdk-a6830ad5d9d62cd9680df48b08de2f493ad119bd/",
    urls = ["https://github.com/intrinsic-ai/sdk/archive/a6830ad5d9d62cd9680df48b08de2f493ad119bd.tar.gz"],
)

bazel_dep(name = "ros2_hwm_asset")
# Use the copy of icon-hwm-controller we've already checked out earlier.
local_path_override(
    module_name = "ros2_hwm_asset",
    path = "../icon-hwm-controller",
)

bazel_dep(name = "platforms", version = "1.1.0")
bazel_dep(name = "protobuf", version = "36.0.bcr.1", repo_name = "com_google_protobuf")
```

</details>

<details> <summary><strong>BUILD</strong></summary>

```bazel load("@ai_intrinsic_sdks//bazel:container.bzl", "container_image",
"container_import") load("@ai_intrinsic_sdks//bazel:python_oci_image.bzl",
"python_layers")
load("@ai_intrinsic_sdks//intrinsic/assets/services/build_defs:services.bzl",
"intrinsic_service")
load("@ai_intrinsic_sdks//intrinsic/icon/hal/bzl:resources.bzl",
"hardware_module_manifest")

package(default_visibility = ["//visibility:public"])

container_import(
    name = "rrbot_ros2_icon_hwm_oci",
    # Export this image from your local docker registry using
    # docker image save rrbot_ros2_icon_hwm:latest -o icon_hwm.tar
    tarball = "icon_hwm.tar",
)

# Next, combine the ROS2 container with the Intrinsic entry point
entrypoint_layers = python_layers(
    name = "entrypoint_layers",
    binary = "@ros2_hwm_asset//icon_hwm_controller:entrypoint_bin",
)

container_image(
    name = "rrbot_ros2_icon_hwm_image",
    base = ":rrbot_ros2_icon_hwm_oci",
    entrypoint = ["/icon_hwm_controller/entrypoint_bin"],
    layers = entrypoint_layers,
    # These are necessary because the entrypoint image is from a
    # different bazel module, so paths are prefixed
    symlinks = {
        "/icon_hwm_controller/entrypoint_bin.runfiles": "/ros2_hwm_asset+/icon_hwm_controller/entrypoint_bin.runfiles",
        "/ros2_hwm_asset+/icon_hwm_controller/entrypoint_bin.runfiles/_main": "/ros2_hwm_asset+/icon_hwm_controller/entrypoint_bin.runfiles/ros2_hwm_asset+",
        "/ros2_hwm_asset+/icon_hwm_controller/entrypoint_bin.runfiles/protobuf+/python/google/api": "/ros2_hwm_asset+/icon_hwm_controller/entrypoint_bin.runfiles/googleapis+/google/api",
    },
)

# The manifest tells the Intrinsic platform which capabilities a service has
hardware_module_manifest(
    name = "rrbot_ros2_icon_hwm_manifest",
    image = ":rrbot_ros2_icon_hwm_image.tar",
    image_sim = ":rrbot_ros2_icon_hwm_image.tar",
    manifest = "proto/rrbot_ros2_icon_hwm_manifest.textproto",
)

intrinsic_service(
    name = "rrbot_ros2_icon_hwm_service",
    default_config = "proto/rrbot_ros2_icon_hwm_default_config.textproto",
    images = [
        ":rrbot_ros2_icon_hwm_image.tar",
    ],
    manifest = ":rrbot_ros2_icon_hwm_manifest",
    deps = [
        "@ai_intrinsic_sdks//intrinsic/assets/services/proto/v1:service_state_proto",
        "@ai_intrinsic_sdks//intrinsic/icon/hal/proto:hardware_module_config_proto",
        "@ros2_hwm_asset//icon_hwm_controller:ros2_hwm_config_proto",
    ],
)
```

</details>

Next, create the two new files in the `proto` directory that the `BUILD` file
references:

The [`ServiceManifest`
proto](/intrinsic_apis/intrinsic/assets/services/proto/service_manifest.proto#L172)
tells the Intrinsic platform how to run your service, and also contains metadata
about an Intrinsic service, like the name and vendor, as well as a short
description.

In this case, you only need to manually provide the metadata, since the
[`intrinsic_service`
rule](/intrinsic/assets/services/build_defs/services.bzl#L102)
fills in the functional parts of the manifest. Check out the proto definition
for `ServiceManifest` and its submessages to see some of the advanced options,
like offering
[gRPC](/intrinsic_apis/intrinsic/assets/services/proto/service_manifest.proto#L58)
and
[HTTP](/intrinsic_apis/intrinsic/assets/services/proto/service_manifest.proto#L77)
servers.

<details>
<summary><strong>rrbot_ros2_icon_hwm_manifest.textproto</strong></summary>

```textproto
# Fill in your name below.
metadata {
  id {
    package: "org.ros.example"
    name: "rrbot_ros2_icon_hwm"
  }
  vendor {
    display_name: "$YOUR_NAME"
  }
  documentation {
    description: "RRBot ROS 2 Control Hardware Module."
  }
  display_name: "RRBot ROS 2 HWM"
}
```
</details>

The default configuration is what new instances of a service start out with. You
should craft this so that things work out of the box as much as possible and,
failing that, add helpful comments to make it easy for users to fill in any
missing parts.

The configuration for a service is an [`Any`
proto](https://github.com/protocolbuffers/protobuf/blob/main/src/google/protobuf/any.proto)
because each service can have its own configuration message. That said, Hardware
Modules (HWMs) all use
[`intrinsic_proto.icon.HardwareModuleConfig`](/intrinsic_apis/intrinsic/icon/hal/proto/hardware_module_config.proto#L27)
because they share some configuration options.

That proto again has an `Any` member called `module_config` for HWM-specific
configuration. For `icon_hwm_controller` HWMs, `module_config` is an
[`intrinsic_proto.services.Ros2HwmConfig`](https://github.com/intrinsic-ai/icon-hwm-controller/blob/main/icon_hwm_controller/proto/ros2_hwm_config.proto)
proto, which tells the entry point script which launch file to start, and what
additional launch arguments to set.

<details>
<summary><strong>rrbot_ros2_icon_hwm_default_config.textproto</strong></summary>

```textproto
# proto-file: google/protobuf/any.proto
# proto-message: google.protobuf.Any

[type.googleapis.com/intrinsic_proto.icon.HardwareModuleConfig] {
  drives_realtime_clock: true
  # cf. https://github.com/ros-controls/ros2_control_demos/blob/master/example_1/bringup/config/rrbot_controllers.yaml#L4
  control_frequency_hz: 50.0
  module_config {
    [type.googleapis.com/intrinsic_proto.services.Ros2HwmConfig] {
      launch_package: "rrbot_ros2_icon_hwm"
      launch_file: "rrbot.launch.py"
      # rrbot doesn't need them, but for more complex launch files you can 
      # define additional launch arguments like so (each entry of launch_parameters
      # consists of a string key and a string value):
      # launch_parameters {
      #   key: "ip_address"
      #   value: "192.170.10.1"
      # }
      # launch_parameters {
      #   key: "num_joints"
      #   value: "2"
      # }
      icon_hwm_controller_config {
        lock_memory: true
        shm_namespace: ""
        realtime_priority_low: 40
        realtime_priority_high: 45
      }
    }
  }
}
```
</details>

With these files in place, you can build your service:

```bash
bazel build rrbot_ros2_icon_hwm_service
```

You should see output that ends with something like this:

```bash
Target //:rrbot_ros2_icon_hwm_service up-to-date:
  bazel-bin/rrbot_ros2_icon_hwm_service.bundle.tar
INFO: Elapsed time: 90.580s, Critical Path: 89.62s
INFO: 17 processes: 2 internal, 15 linux-sandbox.
INFO: Build completed successfully, 17 total actions
```

#### Convert the RRbot URDF to SDF, and make an `intrinsic_scene_object`

The [RRbot
description](https://github.com/ros-controls/ros2_control_demos/blob/master/ros2_control_demo_description/rrbot/urdf/rrbot_description.urdf.xacro)
is relatively simple, but makes heavy use of
[`xacro`](https://github.com/ros/xacro) to generate a URDF file.

If you're on a Ubuntu machine that has ROS package source set up, you can
install `xacro` and `sdformat` like this:

```bash
sudo apt install ros-kilted-sdformat-vendor ros-kilted-xacro
```

After that, you can convert your xacro to URDF:

```bash
xacro rrbot.urdf.xacro > rrbot.urdf
```

... and then convert the URDF to SDF:

```bash
gz sdf --print rrbot.urdf > rrbot.sdf
```

For RRbot, the resulting SDF file looks like this:

<details> <summary><strong>rrbot.sdf</strong></summary>

```xml
<sdf version='1.12'>
  <model name='2dof_robot'>
    <joint name='base_joint' type='fixed'>
      <pose relative_to='__model__'>0 0 0 0 0 0</pose>
      <parent>world</parent>
      <child>base_link</child>
    </joint>
    <link name='base_link'>
      <pose relative_to='base_joint'>0 0 0 0 0 0</pose>
      <inertial>
        <pose>0 0 1 0 0 0</pose>
        <mass>1</mass>
        <inertia>
          <ixx>0.33416666666666661</ixx>
          <ixy>0</ixy>
          <ixz>0</ixz>
          <iyy>0.33416666666666661</iyy>
          <iyz>0</iyz>
          <izz>0.001666666666666667</izz>
        </inertia>
      </inertial>
      <collision name='base_link_collision'>
        <pose>0 0 1 0 0 0</pose>
        <geometry>
          <box>
            <size>0.10000000000000001 0.10000000000000001 2</size>
          </box>
        </geometry>
      </collision>
      <visual name='base_link_visual'>
        <pose>0 0 1 0 0 0</pose>
        <geometry>
          <box>
            <size>0.10000000000000001 0.10000000000000001 2</size>
          </box>
        </geometry>
        <material>
          <diffuse>1 0.529411793 0.0490196086 1</diffuse>
          <ambient>1 0.529411793 0.0490196086 1</ambient>
        </material>
      </visual>
    </link>
    <joint name='joint1' type='revolute'>
      <pose relative_to='base_link'>0 0.10000000000000001 1.95 0 0 0</pose>
      <parent>base_link</parent>
      <child>link1</child>
      <axis>
        <xyz>0 1 0</xyz>
        <limit>
          <effort>100</effort>
          <velocity>1</velocity>
          <lower>-inf</lower>
          <upper>inf</upper>
        </limit>
        <dynamics>
          <damping>0.69999999999999996</damping>
          <friction>0</friction>
          <spring_reference>0</spring_reference>
          <spring_stiffness>0</spring_stiffness>
        </dynamics>
      </axis>
    </joint>
    <link name='link1'>
      <pose relative_to='joint1'>0 0 0 0 0 0</pose>
      <inertial>
        <pose>0 0 0.45000000000000001 0 0 0</pose>
        <mass>1</mass>
        <inertia>
          <ixx>0.084166666666666667</ixx>
          <ixy>0</ixy>
          <ixz>0</ixz>
          <iyy>0.084166666666666667</iyy>
          <iyz>0</iyz>
          <izz>0.001666666666666667</izz>
        </inertia>
      </inertial>
      <collision name='link1_collision'>
        <pose>0 0 0.45000000000000001 0 0 0</pose>
        <geometry>
          <box>
            <size>0.10000000000000001 0.10000000000000001 1</size>
          </box>
        </geometry>
      </collision>
      <visual name='link1_visual'>
        <pose>0 0 0.45000000000000001 0 0 0</pose>
        <geometry>
          <box>
            <size>0.10000000000000001 0.10000000000000001 1</size>
          </box>
        </geometry>
        <material>
          <diffuse>1 1 0 1</diffuse>
          <ambient>1 1 0 1</ambient>
        </material>
      </visual>
    </link>
    <joint name='joint2' type='revolute'>
      <pose relative_to='link1'>0 0.10000000000000001 0.90000000000000002 0 0 0</pose>
      <parent>link1</parent>
      <child>link2</child>
      <axis>
        <xyz>0 1 0</xyz>
        <limit>
          <effort>100</effort>
          <velocity>1</velocity>
          <lower>-inf</lower>
          <upper>inf</upper>
        </limit>
        <dynamics>
          <damping>0.69999999999999996</damping>
          <friction>0</friction>
          <spring_reference>0</spring_reference>
          <spring_stiffness>0</spring_stiffness>
        </dynamics>
      </axis>
    </joint>
    <link name='link2'>
      <pose relative_to='joint2'>0 0 0 0 0 0</pose>
      <inertial>
        <pose>0 0 0.45000000000000001 0 0 0</pose>
        <mass>1</mass>
        <inertia>
          <ixx>0.084166666666666667</ixx>
          <ixy>0</ixy>
          <ixz>0</ixz>
          <iyy>0.084166666666666667</iyy>
          <iyz>0</iyz>
          <izz>0.001666666666666667</izz>
        </inertia>
      </inertial>
      <collision name='link2_collision'>
        <pose>0 0 0.45000000000000001 0 0 0</pose>
        <geometry>
          <box>
            <size>0.10000000000000001 0.10000000000000001 1</size>
          </box>
        </geometry>
      </collision>
      <visual name='link2_visual'>
        <pose>0 0 0.45000000000000001 0 0 0</pose>
        <geometry>
          <box>
            <size>0.10000000000000001 0.10000000000000001 1</size>
          </box>
        </geometry>
        <material>
          <diffuse>1 0.529411793 0.0490196086 1</diffuse>
          <ambient>1 0.529411793 0.0490196086 1</ambient>
        </material>
      </visual>
    </link>
    <frame name='tool_joint' attached_to='link2'>
      <pose>0 0 1 0 0 0</pose>
    </frame>
    <frame name='tool_link' attached_to='tool_joint'>
      <pose>0 0 0 0 0 0</pose>
    </frame>
  </model>
</sdf>
```

</details>

This already contains most of the information we need. Add the missing Intrinsic
data:

* First, remove the `base_joint`:

  ```xml
  <!-- remove this from the .sdf file>
  <joint name='base_joint' type='fixed'>
    <pose relative_to='__model__'>0 0 0 0 0 0</pose>
    <parent>world</parent>
    <child>base_link</child>
  </joint>

  <!-- and also this line from the base_link joint -->
  <pose relative_to='base_joint'>0 0 0 0 0 0</pose>
  ```
* Then, make sure to set up the `intrinsic` namespace as part of the initial
  `<sdf>` tag:

  ```xml
  <sdf version="1.12" xmlns:intrinsic="https://intrinsic.ai/">
  ```
* Next, attach a `flange` frame to the final link of your robot (for RRbot, this
  is ). The Intrinsic realtime control service uses this for Cartesian control,
  and for attaching things to the robot.

  ```xml
  <frame name="flange"
         attached_to="tool_link"
         intrinsic:create_attachment_entity="true">
  </frame>
  ```

* Finally, you can (but don't have to) extend the `<limit>` tag for each joint
  with acceleration and jerk limit values:

  ```xml
  <intrinsic:acceleration>20.79</intrinsic:acceleration>
  <intrinsic:jerk>4747.61</intrinsic:jerk>
  ```

Now you can copy the SDF file to your bazel workspace, and add an
[`sdf_scene_object`](/intrinsic/scene/build_defs/sdf_scene_object.bzl#L133) rule
for it:

```bazel
load("@ai_intrinsic_sdks//intrinsic/scene/build_defs:sdf_scene_object.bzl", "sdf_scene_object")

sdf_scene_object(
    name = "rrbot_sdf_scene_object",
    src = "rrbot.sdf",
)
```

The RRbot SDF file doesn't have any meshes, so we do not need to do anything
special. If your robot does use meshes, you need to list them in the
`sdf_scene_object` rule's `sdf_assets` parameter.  Then you can import them
using their bazel workspace path:

```xml
<geometry>
  <mesh>
    <!-- If your mesh is in meshes/visual 
         (below the folder that has MODULE.bazel): -->
    <uri>model://meshes/visual/visual_1.glb</uri>
  </mesh>
</geometry>
```

Next up, the
[`intrinsic_scene_object`](/intrinsic/assets/scene_objects/build_defs/scene_object.bzl#L115)
rule. This creates another asset that you can install and deploy on your
cluster, or fuse with the service asset you created earlier to make an
`intrinsic_hardware_device`.

But first, we need another manifest:

<details>
<summary><strong>rrbot_scene_object_manifest.textproto</strong></summary>

```textproto
# Again, fill in your name below.
metadata {
  id {
    package: "org.ros.example"
    name: "rrbot_scene_object"
  }
  vendor {
    display_name: "$YOUR_NAME"
  }
  documentation {
    description: "Scene object for RRbot.\n"
  }
  display_name: "RRbot Scene Object"
}
```
</details>

Brilliant! Now add the bazel rule:


```bazel
load("@ai_intrinsic_sdks//intrinsic/assets/scene_objects/build_defs:scene_object.bzl", "intrinsic_scene_object")

intrinsic_scene_object(
    name = "rrbot_scene_object",
    scene_object = ":rrbot_sdf_scene_object",
    manifest = "proto/rrbot_scene_object_manifest.textproto",
)
```

#### Create a HardwareDevice that combines the HWM Service and SceneObject

Almost done! Create an
[`intrinsic_hardware_device`](/intrinsic/assets/hardware_devices/build_defs/hardware_device.bzl#L119)... but
first, you guessed it: One final manifest proto (this one is a bit more
involved):

<details>
<summary><strong>rrbot_hardware_device_manifest.textproto</strong></summary>

```textproto
# Don't forget to fill in your name!
metadata {
  id {
    package: "org.ros.example"
    name: "rrbot_hardware_device"
  }
  vendor {
    display_name: "$YOUR_NAME$"
  }
  documentation {
    description: "A hardware module and scene object for the RRbot ROS example. The hardware module is a wrapped ros2_control launch file."
  }
  display_name: "RRbot Hardware Device"
}
graph {
  nodes {
    key: "scene_object"
    value {
      asset: "org.ros.example.rrbot_scene_object"
    }
  }
  nodes {
    key: "service"
    value {
      asset: "org.ros.example.rrbot_ros2_icon_hwm"
    }
  }
}
```

</details>

```bazel
load("@ai_intrinsic_sdks//intrinsic/assets/hardware_devices/build_defs:hardware_device.bzl", "intrinsic_hardware_device")

intrinsic_hardware_device(
    name = "rrbot_hardware_device",
    assets = [
        ":rrbot_scene_object",
        ":rrbot_ros2_icon_hwm_service",
    ],
    manifest = "proto/rrbot_hardware_device_manifest.textproto",
)
```

#### Build and install!

That's it! You can build and install your new hardware device:

```bash
bazel build :rrbot_hardware_device

inctl asset install bazel-bin/rrbot_hardware_device.bundle.tar --address localhost:17080
```
