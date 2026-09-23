# MoveIt Grasp Planning

The purpose of this tutorial is to showcase an integration of Intrinsic Core with *MoveIt* through [OMTS](../glossary/intrinsic_terms.md#open-machine-tending-solution-omts). We will first set up a ROS colcon workspace to build [*intrinsic-moveit*](https://github.com/intrinsic-ai/intrinsic-moveit). After setting up the OMTS [solution](../glossary/intrinsic_terms.md#solution), we will then re-configure the *flowstate_ros_bridge* [service](../glossary/intrinsic_terms.md#service) to support this integration, before launching the *moveit_planning_service.* When the planning [scene](../glossary/intrinsic_terms.md#scene) on Rviz has been initialized, we can start examples showcasing grasp planning for objects in the scene using *Moveit*.

> [!NOTE]
> Note that this integration is only showing cuboid grasp planning using MoveIt, verified by IK and collision checks, but the motion planning and execution will still be relying on Intrinsic Core capabilities.

![MoveIt Grasp Planning Overview](../../img/learn/tutorials/moveit_grasp_planning_overview.gif)

## 1. Prerequisites and Environment Setup

Users will need to have installed [ROS lyrical](https://docs.ros.org/en/lyrical/Get-Started/Installation/Ubuntu-Install-Debs.html).

Users will first need to build the colcon workspace for *intrinsic-moveit*.

```bash
# Set up the workspace and clone the repository
mkdir -p ~/ws_intrinsic_moveit/src && cd ~/ws_intrinsic_moveit/src
gh repo clone intrinsic-ai/intrinsic-moveit
# Prepare and install the dependencies
source /opt/ros/lyrical/setup.bash
vcs import . < intrinsic-moveit/lyrical.repos
cd ~/ws_intrinsic_moveit
rosdep install --from-paths . --ignore-src -r -y
sudo apt update && sudo apt install -y ros-lyrical-rmw-zenoh-cpp
# Build, this will take a while, time for another cup of coffee
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  --symlink-install --packages-up-to moveit_planning_service
```

Users will then need to start the OMTS [simulation](../glossary/general_terms.md#simulation).

```bash
# Until it gets merged onto main, we need to use the feature branch
cd ~
gh repo clone intrinsic-ai/intrinsic-omts omts-moveit -- \
  --revision=moveit-integration
# Note that this is a different directory from the original omts directory
cd ~/omts-moveit
bazel run //:omts_solution -c opt -- \
  --address localhost:17080 --operation_mode=sim
```

Once the solution is up and running, verify with Rviz that it should look like this.

![MoveIt Initial RViz](../../img/learn/tutorials/moveit_rviz_initial.png)

In order for the integration to work, we will need to reconfigure OMTS’s running *flowstate_ros_bridge* service. The detailed configurations required can be found [here](https://github.com/intrinsic-ai/intrinsic-moveit/blob/main/docs/flowstate_ros_bridge_configuration.md#configuration-requirements).

```bash
# Download the required binaries
cd ~/Downloads/
gh release download v0.0.2 -R intrinsic-ai/intrinsic-moveit \
  -p "moveit_plan_grasp_skill.bundle.tar" \
  -p "flowstate_ros_bridge_config.binarypb"
# Stop flowstate_ros_bridge
inctl service delete --address localhost:17080 flowstate_ros_bridge
# Restart flowstate_ros_bridge with the config, find the path to the binary
inctl service add --address localhost:17080 ai.intrinsic.flowstate_ros_bridge \
  --config ~/Downloads/flowstate_ros_bridge_config.binarypb
```

We will also need the *moveit_plan_grasp_skill* which can be called from OMTS, and interacts with the *moveit_planning_service* to obtain pre-grasps and grasps.

```bash
# Install the planning skill
inctl asset install --address localhost:17080 \
  ~/Downloads/moveit_plan_grasp_skill.bundle.tar
```

We can now start the *moveit_planning_service*.

```bash
# These commands are generally required for all terminals running ROS
source ~/ws_intrinsic_moveit/install/setup.bash
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
export ZENOH_CONFIG_OVERRIDE='mode="client";connect/endpoints=["tcp/127.0.0.1:7447"]'
# Start the service
ros2 launch moveit_planning_service service.launch.py headless:=false \
  start_service_status_monitor:=false
```

Once the planning service launches, verify that the additional Rviz window should look like below. The [robot](../glossary/general_terms.md#robot) is represented by its meshes, while all other objects in the scene are propagated as collision objects and represented as green meshes.

![MoveIt RViz Planning Service](../../img/learn/tutorials/moveit_rviz_planning_service.png)

## 2. OMTS state synced to MoveIt planning scene

The state of the robot and objects are synchronized with the MoveIt planning scene. This can be verified with

```bash
cd ~/omts-moveit
# Jogging the robot, see tutorial "Jog the robot"
bazel run //tools/jogging:jog_interactive -- \
  --host=localhost \
  --port=17080 \
  --instance=icon
# Updating the scene, see tutorial "Cell customization"
bazel run //tools/world:apply_scene_updates -- \
  --address localhost:17080 \
  --files configs/raw_stock_in_vise.updates.pbtxt
# Resetting the scene, see tutorial "Cell customization"
inctl world reset --address localhost:17080
```

![MoveIt State Synchronization](../../img/learn/tutorials/moveit_state_sync.gif)

This gif is sped up 2x.

## 3. Grasp planning on *raw_stock_2x3x5*

![MoveIt Grasp Planning 1](../../img/learn/tutorials/moveit_grasp_planning_1.png)
![MoveIt Grasp Planning 2](../../img/learn/tutorials/moveit_grasp_planning_2.png)

We can run the *moveit_plan_and_move* to plan for each object and optionally move the robot to the pre-grasp frame.

```bash
cd ~/omts-moveit
# Dry run: plan a grasp on raw_stock_2x3x5 without moving the arm
bazel run //third_party/intrinsic_moveit/tools:moveit_plan_grasp_and_move -- \
  --address=localhost:17080 \
  --target_object=raw_stock_2x3x5 \
  --plan_only \
  --surfaces=0,1,4,5
# Plan and approach the pre-grasp on the table surface
bazel run //third_party/intrinsic_moveit/tools:moveit_plan_grasp_and_move -- \
  --address=localhost:17080 \
  --target_object=raw_stock_2x3x5 \
  --surfaces=0,1,4,5
# Optionally, reset the scene such that the trajectory to the CNC Vice is shorter
# inctl world reset --address localhost:17080
# Relocate Workpiece to the CNC Vice
bazel run //tools/world:apply_scene_updates -- \
  --address=localhost:17080 \
  --files configs/raw_stock_in_vise.updates.pbtxt
# Plan again to grasp the workpiece that is in the vice now
# This planning step may take longer due to the length of the trajectory if the
# world has not been reset
bazel run //third_party/intrinsic_moveit/tools:moveit_plan_grasp_and_move -- \
  --address=localhost:17080 \
  --target_object=raw_stock_2x3x5 \
  --surfaces=0,1,4,5
```

![MoveIt Plan and Move](../../img/learn/tutorials/moveit_grasp_planning_overview.gif)

This gif is sped up 2x.

The various motions in the MoveIt planning scene represent the trajectories that MoveIt generated, that can be used to reach the planned pre-grasp frames, ranked by cost. By default, only the best set of grasp and pre-grasp is returned, which will be used to change the positions of the pre-grasp and grasp frames in OMTS.

## 4. Things to Try Next (Next Steps)

- Adding more objects to the scene, e.g. *building_block*, and perform grasp planning on them
- Tune the planner parameters either from the skill execution’s side, or from the values used by the MoveIt Task Constructor (MTC) within *moveit_planning_service*. This should enable more robust grasp planning and IK solving across various scenarios.
- Run through the OMTS cycle, but replace the generic pre-grasp calculation with grasp planning from this integration.
- Set up custom hardware descriptions and MoveIt configs.

## 5. Architecture

The architecture diagram can be seen found [here](https://github.com/intrinsic-ai/intrinsic-omts/tree/ac/grasp-planning-demo/third_party/intrinsic_moveit#architecture).

## 6. Troubleshooting

| Issue | Potential fix |
| --- | --- |
| Planning scene is not synced with OMTS, e.g. arm [joint](../glossary/general_terms.md#joint) angles are wrong, collisions are missing, etc | Verify that the icon, ur_module and flowstate_ros_bridge service are running with `kubectl get pods --all-namespaces`, or restart the flowstate_ros_bridge service with:<br>`inctl service delete --address localhost:17080 flowstate_ros_bridge`<br>`inctl service add --address localhost:17080 ai.intrinsic.flowstate_ros_bridge \`<br>`  --config ~/Downloads/flowstate_ros_bridge_config.binarypb`<br>Or check that joint states are being published, `ros2 topic echo /joint_states --once` |
| Grasp plan not found | This generally occurs when the motion distance is large, or the object is quite occluded (e.g. in the CNC enclosure). Since the default planner uses a sampling based planner, we can increase the planning timeout in moveit_plan_grasp_and_move.py. More configurations to choose planners or configure the planners will be in future releases. |
| Grasp plan not found for different objects | The gripper opening maxes out at 50mm, if the object has widths on all sides that are larger than 50mm, no valid  grasp approach plans will be found, e.g. raw_stock_2x3x5. Try a smaller object,  e.g. building_block. |
| After changing out the hardware (e.g. arm, enclosure, gripper) in Intrinsic Core, the MoveIt planning scene does not match the setup | Intrinsic-moveit currently only supports the hardware setup of OMTS. We will next work on abstracting the base hardware description layer such that users can more easily set up their own hardware description and MoveIt config. |
