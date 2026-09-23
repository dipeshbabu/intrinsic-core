# Swap Assets

The purpose of this tutorial is to illustrate the various ways to swap out Assets in a [Solution](../glossary/intrinsic_terms.md#solution). We will first use the CLI to swap out a software Skill using *inctl*. We will then replace the standard UR [robot](../glossary/general_terms.md#robot) with the bundled KUKA KR10-R1100-2 by altering the Bazel build rules.

## Prerequisites

If you haven't already, follow [Getting started](getting_started.md) to fetch the sources for the [Open Machine Tending Solution (OMTS)](../glossary/intrinsic_terms.md#open-machine-tending-solution-omts) Solution and start it.

## Part 1: Swap a software [Asset](../glossary/intrinsic_terms.md#asset)

[Skills](../glossary/intrinsic_terms.md#skill) are the building blocks of an automation process in Intrinsic Core. They're used to move the robot (move\_robot), control other hardware (dio\_set\_output), capture camera images (capture\_images) and much more. Here, we'll use inctl to replace "clear\_motion\_planner\_service\_cache" with "adder".


1. List the Skills in your Solution:


```bash
inctl skill list --address localhost:17080
```

2. Remove a Skill from the running Solution:


```bash
inctl asset uninstall ai.intrinsic.clear_motion_planner_service_cache \
  --address localhost:17080
```

3. Build a new Skill and install it to the running Solution:


```bash
cd ~/intrinsic-omts
bazel build @intrinsic-core//intrinsic/skills/examples:adder_skill
tarball_path=$(bazel --quiet cquery --output=files @intrinsic-core//intrinsic/skills/examples:adder_skill)
inctl asset install --address localhost:17080 "${tarball_path}"
```

>[!NOTE]
>Skills are different to other Assets in that you don't directly add an instance to the Solution. Instead, when writing a Process that defines the automation task you'll parameterize the Skills, telling them what to do and when to do it.


4. List the Skills again:


```bash
inctl skill list --address localhost:17080
```

You should see "ai.intrinsic.adder" at the start of the list.

5. Because the Skill runs inside [Kubernetes](../glossary/general_terms.md#kubernetes-k8s), we can use k9s, a terminal UI for managing Kubernetes, to check the logs. Check the Skill logs by opening k9s, then selecting `adder` and pressing `l` to see the logs.


```bash
k9s -n skills -c services
```

*The Skill is addressed as a Kubernetes "service" in a namespace called "skills". A Kubernetes service is different from an Intrinsic Service: Where a Kubernetes service is used for anything that receives network requests (including Skills) in Kubernetes, and Intrinsic Service refers to a long-running program such as the driver that controls the UR robot or the Gazebo simulator.*


You should see "Skill service listening on ..."


This workflow uninstalled the old Skill and installed the new one in the running Solution, but because we only changed the running Solution, redeploying the Solution undoes your change. Next, we will change a hardware Asset by modifying the BUILD files that define the Solution.

## Part 2: Swap a hardware Asset

When adding a new robot to the Solution, we'll need to deal with two parts of the Intrinsic Real-Time Control Framework (ICON):

- The Real-Time Control Service (RTCS) controls one or more robots or other real-time devices (eg grippers, digital outputs).
- The Hardware driver module (HWM) interfaces with a specific robot or device, in this case the UR or KUKA robot.
    - Note: The Hardware Driver is an Asset that combines both the software that controls the robot (a "Service") and the geometric models of the robot (the "Scene Object"). This allows Intrinsic Core to calculate collision-free trajectories for the robot and to move it along those trajectories.

You'll replace the Asset that provides the HWM and the Asset instance that configures it, and reconfigure the RTCS for compatibility with the new HWM by changing the configuration referenced by the Asset instance.


Note: All future tutorials assume the robot is the UR. Create a new local Git branch so everything you edit can be easily reverted or stashed before continuing with the remaining tutorials. 


1. Edit the OMTS Solution BUILD file to change the Asset, Asset instance and ICON configuration: *If you prefer, use another editor* *such as "nano" for command-line editing over an SSH session.*


```bash
cd ~/intrinsic-omts
gedit BUILD
```

```python
# Solution deployment definition
intrinsic_solution(
    name = "omts_solution",
    assets = _OMTS_SKILL_ASSETS + [
        [snip: workcell-independent assets]
    ] + select({
        ":is_lab_bb_01": [
            "@intrinsic-core//intrinsic/apps/bluebird_caw/resources:caw_enclosure",
            # "@intrinsic-core//intrinsic_control/intrinsic/icon/hardware_modules/universal_robots:ur3e_hardware_module_core", # REMOVE
            "@intrinsic-core//intrinsic_control/intrinsic/icon/hardware_modules/kuka_rsi:kr10_r1100_2_hardware_module", # ADD
        ],
        "//conditions:default": [
            [snip: assets for a different workcell]
        ],
    }),
    default_operation_mode = "real",
    instances = [
        ":enclosure",
        ":robotiq_pinch_gripper",
        ":raw_stock_2x3x5",
        ":gazebo_simulator",
        ":charuco_9x14_20mm_15mm_dict_5x5",
        ":calibration_service_instance",
        ":icon",
        # ":ur_module", # REMOVE
        ":kuka_rsi_hal_module", # ADD
        ":orbbec_camera",
        ":orbbec_gemini_driver",
        ":motion_planner_service",
        ":inference_service",
        ":pose_estimator_service",
        ":train_service",
        ":flowstate_ros_bridge",
        ":hand_e_gripper_service",
    ] + select({
        [snip: workcell-specific instances]
    }),
    [snip: object_world_updates]
)
[snip: other Asset instances]
intrinsic_asset_instance(
    name = "icon",
    asset = "ai.intrinsic.generic_realtime_control_service",
    instance_name = "icon",
    service_config = select({
        # ":is_lab_bb_01": "//configs:lab_bb_01/icon_config.textproto", # REMOVE
        ":is_lab_bb_01": "//configs:kr_10/icon_config.textproto", # ADD
        "//conditions:default": "//configs:omts/icon_config.textproto",
    }),
)
# ADD
intrinsic_asset_instance(
    name = "kuka_rsi_hal_module",
    asset = "ai.intrinsic.kuka_kr10_hardware_module",
    instance_name = "kuka_rsi_hal_module",
    service_config = ":kuka_rsi_config.textproto",
)
# REMOVE
# intrinsic_asset_instance(
#     name = "ur_module",
#     asset = select({
#         ":is_lab_bb_01": "ai.intrinsic.ur3e_hardware_module_core",
#         "//conditions:default": "ai.intrinsic.ur5e_hardware_module_core",
#     }),
#     instance_name = "ur_module",
#     service_config = select({
#         ":is_lab_bb_01": "//configs:lab_bb_01/ur_module_config.textproto",
#         "//conditions:default": "//configs:omts/ur_module_config.textproto",
#     }),
# )
```
2. Copy the [default KUKA RSI config](../../../intrinsic_control/intrinsic/icon/hardware_modules/kuka_rsi/kuka_rsi_default_config.textproto) to the Solution directory. *If connecting to a real robot, this file is where you would configure the IP addresses and other Solution-specific settings.*


```bash
cd ~/intrinsic-omts
cp ~/intrinsic-core/intrinsic_control/intrinsic/icon/hardware_modules/kuka_rsi/kuka_rsi_default_config.textproto \
  kuka_rsi_config.textproto
```

3. We have provided a configuration for ICON's real-time control service that works with the KUKA KR10. To learn more about how ICON communicates with the hardware module, review the differences between the configurations:


```bash
cd ~/intrinsic-omts
diff configs/lab_bb_01/icon_config.textproto configs/kr_10/icon_config.textproto
```

Notice how:
  
  a. The ICON config refers to the hardware module's Asset instance by name, so every `ur_module` becomes `kuka_rsi_hal_module`.
  
  b. We've changed the control frequency from the UR's 500 Hz to the KR10's 250 Hz.

  c. We've adjusted the analog and digital input/output configuration to match the I/Os on the KR10.

  d. We've removed the force/torque (FT) sensor, as this information is not provided by the KR10. To use force-based insertion with the KR10, you'd need to add an external FT sensor.



4. Adjust ObjectWorldUpdates to use the name of the new robot instance:


```bash
cd ~/intrinsic-omts
sed -i "s/ur_module/kuka_rsi_hal_module/g" configs/lab_bb_01/*.updates.pbtxt
```

5. Stop the old Solution and start the Solution with the KUKA robot:


```bash
cd ~/intrinsic-omts
inctl solution stop --address localhost:17080
bazel run //:omts_solution --config=lab_bb_01 -- \
  --address localhost:17080 --operation_mode=sim
```

*If you see `No object with name "ur_module" exists.`, check that you cleaned up `relocate_robot.updates.pbtxt` from the previous tutorial, and that you successfully rewrote the other ObjectWorldUpdates with sed in the previous step: `grep ur_module configs/lab_bb_01/*.updates.pbtxt` should print nothing. (The `configs/omts/` files still refer to `ur_module`, but they aren't used when deploying with `--config=lab_bb_01`.)*

6. Check that the new robot is operational:


```bash
inctl icon status --address localhost:17080 --instance_name icon
```

*If you see FAULTED, check* *Troubleshooting* *below for possible fixes.*


7. Close and reopen Gazebo or RViz (check [Visualize the robot](visualize_the_robot.md) for details). You should see the KUKA robot in both views.

   ![KUKA robot swap in RViz](../../img/learn/tutorials/kuka_robot_swap.png)




8. Clean up your changes so that they do not interfere with other tutorials:


```bash
cd ~/intrinsic-omts
rm kuka_rsi_config.textproto
git checkout BUILD configs/lab_bb_01/*.updates.pbtxt
inctl solution stop --address localhost:17080
bazel run //:omts_solution --config=lab_bb_01 -- \
  --address localhost:17080 --operation_mode=sim
```
## Troubleshooting



| Symptom | Cause | Resolution |
| --- | --- | --- |
| Error: deploy application: rpc error: code = Internal desc = conductor failed to start solution: rpc error: code = NotFound desc = CreateWorldFromResourceSetData: rpc error: code = NotFound desc = No object with name "ur_module" exists.;  Unable to resolve ObjectReference with debug hint: ""; could not load resource set into edit world | The ObjectWorldUpdates refer to an instance "ur_module" which is not found. | Adjust the Asset instance name or the ObjectWorldUpdates so that they match. |
| Fault Reason:      FAILED_PRECONDITION: Inconsistent configuration with Hardware Module 'kuka_rsi_hal_module'. ICON ('control_frequency_hz'): 2000000 ns (500.0 Hz), hardware module reports: 4000000 ns (250.0 Hz). Check your configuration. | The Real-time Control Service is configured with a different control frequency from the Hardware Module. | Adjust icon_config.textproto to match the frequency in kuka_rsi_config.textproto. |
| Fault Reason:      NOT_FOUND: Hardware module 'kuka_rsi_hal_module' does not export interface 'analog_input_status'. Only provides: [...]; Error building Part 'adio' | The Real-time Control Service is configured to expect an interface that the Hardware Module does not provide. | Adjust icon_config.textproto to refer only to interfaces listed in the error message. Remove any other interfaces, or remove entries from "parts_by_name" if they refer to capabilities this robot doesn't have, such as a force/torque sensor. |
| Fault Reason:      NOT_FOUND: No object with name "kuka_rsi_hal_module" exists.;  Unable to resolve ObjectReference with debug hint: ""; Error building Part 'arm'. | The instance name is not consistent between the configurations for the Solution, the real-time control service and the hardware module. | Adjust BUILD, icon_config.textproto, and/or kuka_rsi_config.textproto to use a consistent interface name across all three. |
| Gazebo shows a robot with disconnected links. | The kinematic model loaded by the Gazebo GUI does not match the model used by the Gazebo server. | Restart the Gazebo GUI. |
| RViz shows a UR robot after switching to the KUKA robot. | RViz has either failed to receive the latest models, or is configured to use a fixed model. | Check that RViz has a MarkerArray display, not a RobotModel display. Check Visualization Troubleshooting for communication problems. |

## Next steps

Notice that the KR10 is much larger than the UR robot. Next, you will learn to [import a new part](import_new_part.md), which you can use to import an enclosure large enough for the KR10.

