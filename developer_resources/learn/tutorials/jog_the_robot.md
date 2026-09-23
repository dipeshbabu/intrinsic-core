# Jog the robot

Follow these instructions to jog (manually move) a real or simulated [robot](../glossary/general_terms.md#robot).

## Prerequisites

If you haven't already, follow [Getting started](getting_started.md) to fetch the sources for [Open Machine Tending Solution (OMTS)](../glossary/intrinsic_terms.md#open-machine-tending-solution-omts) [Solution](../glossary/intrinsic_terms.md#solution) and start it, and open [RViz or Gazebo](visualize_the_robot.md) to visualize the robot.

Verify that the Solution is reachable by listing the [Asset](../glossary/intrinsic_terms.md#asset) instances:

> [!NOTE]
> We'll explain Assets and Asset instances in more detail in the [Swap Assets](swap_assets.md) tutorial.

```bash
inctl asset instances list --address localhost:17080
```

The output should include an instance "icon" of the generic_realtime_control_service. If you're using a custom Solution, the instance may have another name, which you'll need to use in the commands below. Example output:

```text
Name                            Asset
calibration_service             ai.intrinsic.calibration_service
charuco_9x14_20mm_15mm_dict_5x5 ai.intrinsic.charuco_9x14_20mm_15mm_dict_5x5
[snip: other Asset instances]
icon                            ai.intrinsic.generic_realtime_control_service
[snip: other Asset instances]
ur_module                       ai.intrinsic.ur3e_hardware_module_core
```

Verify that the [Intrinsic Real-Time Control Framework (ICON)](../glossary/intrinsic_terms.md#intrinsic-real-time-control-framework-icon) is operational:

```bash
inctl icon status --address localhost:17080 --instance_name icon
```

If you see "ENABLED" like in the output below, start the jogging tool.

```text
Operational Status: ENABLED
[...]
```

> [!NOTE]
> If you get the error:
> ```text
> Error: rpc error: code = Unimplemented desc =
> ```
>
> This can have one of two causes:
> * **The instance_name is wrong**: Compare the value of `--instance_name` against the output of `inctl asset instances list --address localhost:17080`.
> * **ICON and ur_module are not running**: This might be because you didn't specify `--operation_mode=sim` on a simulation PC, or because you haven't run `setup_realtime.sh` to set up the PC for execution on a real robot. Check the root cause with k9s, looking at pods in the "resources" namespace:
>
>   ```bash
>   k9s -n app-resources -c pods
>   ```
>
>   Press Down to select the `rs-icon-0` pod. If it's "Pending" with a message of `0/1 nodes are available: 1 node(s) didn't match Pod's node affinity/selector` you should either:
>   * to use a simulated robot, restart the application with `--operation_mode=sim`
>   * to use a real robot, run `sudo ~/intrinsic-core/intrinsic_runtime/setup_realtime.sh` and reboot
>
>   You may need to use k9s to delete the Pending pods before your changes are applied: Select them and press Ctrl+D, then select OK and press Enter. You should see the "age" start counting from zero.
>
>   Otherwise, press `l` to check the logs.

> [!NOTE]
> If you get the error:
> ```text
> Operational Status: FAULTED
> Fault Reason:      ABORTED: ReadStatus failed: 'ur_module': The server is gone, cannot trigger any more requests
> ```
>
> This indicates that ICON has faulted due to a restart of the Gazebo simulation. For real robots, you'll see fault reasons that correspond to the status of the robot [controller](../glossary/general_terms.md#controller). Try clearing faults:
>
> ```bash
> inctl icon clear-faults --address localhost:17080 --instance_name icon
> ```
>
> If this fails, you may need to try `clear-faults` again (some faults hide other faults), disable e-stop, etc.

## Start the jogging tool

The jog_interactive tool allows direct keyboard control of robot [joints](../glossary/general_terms.md#joint).

```bash
cd ~/intrinsic-omts
bazel run //tools/jogging:jog_interactive -- \
  --host=localhost \
  --port=17080 \
  --instance=icon
```

### Controls

* **Select Joint**: Enter a joint number (0 to 5 on a 6-DoF arm) or q to quit.
* **Jog Joint**:
  * **Right Arrow (->)**: Jog +0.01 rad
  * **Left Arrow (<-)**: Jog -0.01 rad
  * **q or x**: Return to joint selection menu.

> [!NOTE]
> If you get the error:
> ```text
> ICON RPC error: <_MultiThreadedRendezvous of RPC that terminated with:
>         status = StatusCode.FAILED_PRECONDITION
>         details = "Part: 'arm' is already in use."
> >
> ```
>
> This indicates that ICON is already being controlled by another client. You can forcibly end the other session by disabling and reenabling ICON:
>
> ```bash
> inctl icon disable --address localhost:17080 --instance_name=icon
> inctl icon enable --address localhost:17080 --instance_name=icon
> ```

> [!NOTE]
> If you get the error:
> ```text
> Action error: Adding actions failed with grpc.StatusCode.INVALID_ARGUMENT - Goal violates Part limits. [...]
> ```
>
> This indicates that you've jogged the robot too far. Simply stop and restart the jog_interactive tool, or prompt your coding agent to iterate on a jogging tool that respects joint limits.

## What's next

If you're curious, you can [look at the source code](https://github.com/intrinsic-ai/intrinsic-omts/blob/main/tools/jogging/jog_interactive.py) of the jogging tool to learn how it communicates with ICON by adjusting the goal position.

Then, proceed to [Visualize the Solution](visualize_the_solution.md) to observe the Open Machine Tending Solution as it loads and unloads a simulated CNC machine.
