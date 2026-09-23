# Cell customization

Learn how to customize the physical layout of your [Open Machine Tending Solution (OMTS)](../glossary/intrinsic_terms.md#open-machine-tending-solution-omts) cell, including the [robot](../glossary/general_terms.md#robot) base and key frames.

## Prerequisites

If you haven't already, follow [Getting started](getting_started.md) to fetch the sources for OMTS [Solution](../glossary/intrinsic_terms.md#solution) and start it, and follow [Visualize the robot](visualize_the_robot.md) to start RViz, which we'll use to verify changes to the layout.

## Understanding ObjectWorldUpdates

An Intrinsic Solution defines objects by providing [Asset](../glossary/intrinsic_terms.md#asset) instances that make up the Solution. For example, if a Universal Robot (UR) manipulator will pick up a building block, your Solution might look like this simplified example:


```python
intrinsic_solution(
    name = "my_solution",
    # Assets are the "types" of objects in your Solution.
    # Later, we'll talk about Skills and Services - these are Assets too.
    assets = [
        "//assets/building_block",
        "//assets/gripper",
        "//assets/ur5e",
    ],
    # An instance combines the Asset with configuration.
    # You can have more than one instance of each Asset.
    instances = [
        "//assets/building_block_instance",
        "//assets/gripper_instance",
        "//assets/ur5e_instance",
    ],
    # ObjectWorldUpdates describe the geometric relationship between objects.
    object_world_updates = [
        "//configs:scene.updates.pbtxt",
    ],
)
```

*This is a snippet from a Bazel BUILD file, which defines a "target" (a build input or output) called "my\_solution" based on other targets (starting with //...). The object\_world\_updates list refers to a target "//configs:scene.updates.pbtxt" - this is a file called scene.updates.pbtxt in the repo subdirectory called "configs".*


The object_world_updates attribute provides a list of configuration files that define the relationships and initial positions of every object, for example that the gripper is attached to the robot, and where the building block is.


These files also define "frames", which represent conceptual positions that can change position during execution. For example, the "grasp_frame" indicates where the robot should move to in order to pick up the building block, so it might have a fixed position relative to the building block, or it might change depending on which side of the building block is facing up.

### Supported Update Types

This list includes common uses of ObjectWorldUpdates. For the full list, refer to [object\_world\_updates.proto](https://github.com/intrinsic-ai/sdk/blob/main/intrinsic/world/proto/object_world_updates.proto).


- **create\_frame**: Creates a new named coordinate frame attached to a parent object or frame.
- **update\_transform:** Updates the relative translation and orientation between two existing nodes (objects or frames).
- **reparent\_object**: Modifies the kinematic parent of an object (such as mounting a gripper onto a robot [flange](../glossary/general_terms.md#flange)).
- **update\_object\_joints**: Sets initial or default [joint](../glossary/general_terms.md#joint) positions for an articulated robot.

## Live update

We'll start off by applying updates directly to a running Solution. These changes will be saved in memory, so if you reset the world or stop and restart the Solution, they'll be gone. You'll see later how to save them in the Solution.

### Example 1: Add a New Target Frame

We'll create a new frame that could be used to capture an image of an object before grasping.


1. Add [coordinate frames](../glossary/general_terms.md#frame) to the RViz display:

   a. Displays > Add > TF > OK.
   
   b. Verify that you see the blue/green/red coordinate frames:

   ![RViz TF coordinate frames](../../img/learn/tutorials/rviz_tf_frames.png)



2. Create a configuration file defining a [pose](../glossary/general_terms.md#pose) in the robot's workspace:
>[!NOTE]
>The OMTS solution has a frame with a similar purpose called "view". We're using a different name here to avoid a conflict. If you haven't used Protocol Buffers before, this [*text format*](https://protobuf.dev/reference/protobuf/textformat-spec/) will be unfamiliar. The "proto-file" and "proto-message" headers help you identify the schema that defines the possible values, [*ObjectWorldUpdates*](https://github.com/intrinsic-ai/sdk/blob/7e76fa3c7f34618074bfb3556dea1db1911c1bb7/intrinsic/world/proto/object_world_updates.proto#L64) in this case. The rest is a nested structure similar to JSON or YAML, although you should be warned about the [*surprising syntax for lists*](https://protobuf.dev/reference/protobuf/textformat-spec/#:~:text=Fields%20marked%20repeated%20can%20have%20multiple%20values), known as "repeated fields".


```textproto
cat > /tmp/capture_frame.updates.pbtxt << EOF
# proto-file: intrinsic/world/public/proto/object_world_updates.proto
# proto-message: intrinsic_proto.world.ObjectWorldUpdates
updates: {
  create_frame: {
    new_frame_name: "capture"
    # The new frame is positioned relative to the origin ("root").
    parent_object_with_filter: {
      reference: {
        by_name: {
          object_name: "root"
        }
      }
    }
    # _t_ indicates the transform from the parent (root) to the new frame.
    parent_t_new_frame: {
      position: {
        x: 0.3
        y: 0.0
        z: 1.2
      }
      orientation: {
        x: 0.0
        y: 0.0
        z: 0.0
        w: 1.0
      }
    }
  }
}
EOF
```
3. Apply the frame into the running Solution:


```bash
cd ~/intrinsic-omts
bazel --quiet run //tools/world:apply_scene_updates -- \
  --address localhost:17080 \
  --files /tmp/capture_frame.updates.pbtxt
```

4. To simplify the view in RViz, remove the other frames:

   a. Displays > TF > Frames > disable All Enabled
   
   b. scroll to the bottom and select root/capture
   
   c. Verify that you see a single frame:

   ![RViz with capture frame](../../img/learn/tutorials/rviz_capture_frame.png)


5. Reset the state of the world and [simulation](../glossary/general_terms.md#simulation): After this starts, switch to RViz to watch the result.


```bash
inctl world reset --address localhost:17080
```

You should see the frame fade out of view, indicating that your updates have been reset.


<details>

  <summary>Why does "inctl world reset" undo these changes?</summary>

  In addition to the "belief world" and "simulated world" described in [World concepts](../platform_introduction/world_concepts.md), Intrinsic Core records an "initial world" whenever you deploy a Solution. This was created when you deployed the Solution using the object\_world\_updates files referenced by the intrinsic\_solution() target in the BUILD files. It gets recorded in the Object World [Service](../glossary/intrinsic_terms.md#service), which keeps track of the initial and belief worlds.

  When you run "inctl world reset", Intrinsic Core resets the belief world to match the initial world, and resets the simulated world as well.

</details>


For an extra challenge, try adjusting the pose of the frame so that the blue (Z) axis points at the block. You can use the [3D Rotation Converter](https://www.andre-gaschler.com/rotationconverter/) to turn a rotation into the \[w, x, y, z\] parameters of the quaternion.

### Example 2: Relocate Robot Base

We'll adjust the position of the robot relative to its environment. Getting this pose right is important when modelling the enclosure, so that generated trajectories don't cause the robot to collide with its surroundings.


1. Check the current state of the world:


```bash
cd ~/intrinsic-omts
bazel --quiet run //tools/world:inspect_world -- --address localhost:17080
```

Note down the position of the flange: We expect it to move along with the robot. 

2. Create a base transform file:
  
This transform says that we're changing the relationship between (a) the origin of the scene (root) and (b) the UR robot (ur\_module). We're doing this by moving (ur\_module) to the position \[0.5, -0.3, 1.2\] relative to the origin.


```textproto
cat > /tmp/relocate_robot.updates.pbtxt << EOF
# proto-file: intrinsic/world/public/proto/object_world_updates.proto
# proto-message: intrinsic_proto.world.ObjectWorldUpdates
updates: {
  update_transform: {
    # Change the transform between two objects:
    #   a: the origin ("root")
    #   b: the robot ("ur_module")
    node_a {
      by_name {
        object {
          object_name: "root"
        }
      }
    }
    node_b {
      by_name {
        object {
          object_name: "ur_module"
        }
      }
    }
    node_to_update: {
      by_name {
        object {
          object_name: "ur_module"
        }
      }
    }
    # As before, a_t_b is the transform from a's reference frame to b's.
    a_t_b {
      position {
        x: 0.500
        y: -0.300
        z: 1.000
      }
      orientation {
        x: 0.0
        y: 0.0
        z: -0.707
        w: 0.707
      }
    }
  }
}
EOF
```

3. Apply the frame into the running Solution:


```bash
cd ~/intrinsic-omts
bazel --quiet run //tools/world:apply_scene_updates -- \
  --address localhost:17080 \
  --files /tmp/relocate_robot.updates.pbtxt
```

4. Check RViz or Gazebo and you should see that the robot has moved:  

   ![RViz robot relocated temporarily](../../img/learn/tutorials/rviz_robot_relocated_perm.png)


5. Check the updated state of the world:


```bash
cd ~/intrinsic-omts
bazel --quiet run //tools/world:inspect_world -- --address localhost:17080
```

You should see that the flange has a different pose: It has moved along with the robot.


6. Reset the execution state to match the initial world baseline: 

```bash
inctl world reset --address localhost:17080
```

If you have both RViz and Gazebo open, you'll see that the robot returns to the enclosure in RViz before it returns in Gazebo. 


Kinematics and planning targets immediately reflect the updated base pose, but do not persist beyond a Solution restart or world reset. 


## Permanent Customization

When you want the Gazebo physics simulation and RViz visual models to permanently reflect a new robot base pose, you'll need to edit the `intrinsic_solution()` definition. The instructions assume that:


- You've followed the steps above to create /tmp/relocate\_robot.updates.pbtxt.
- You're using the //:omts_solution Solution. If you're using a different Solution, adjust the commands below accordingly.


To change the Solution:


1. Copy the world updates file into the source directory:


```bash
cd ~/intrinsic-omts
cp /tmp/relocate_robot.updates.pbtxt .
```

2. Edit the BUILD file that defines the Solution: If you prefer, use another editor like "nano" for command-line editing over an SSH session.


```bash
cd ~/intrinsic-omts
sudo apt install gedit
gedit BUILD
```

3. Find the block containing "omts_solution" : 

```python
# BEFORE
# Solution deployment definition
intrinsic_solution(
    name = "omts_solution",
    assets = [
        [snip: lots of Assets]
    ],
    default_operation_mode = "real",
    instances = [
        [snip: lots of instances]
    ],
    object_world_updates = select({
        ":is_lab_bb_01": [
            "//configs:lab_bb_01/ur_module.attachments.updates.pbtxt",
            "//configs:lab_bb_01/scene.updates.pbtxt",
            "//configs:lab_bb_01/align_robot.updates.pbtxt",
            "//configs:lab_bb_01/orbbec_gemini.updates.pbtxt",
            # you'll add the new line here
        ],
        "//conditions:default": [
            "//configs:omts/ur_module.attachments.updates.pbtxt",
            "//configs:omts/scene.updates.pbtxt",
            "//configs:omts/align_robot.updates.pbtxt",
            "//configs:omts/cnc_enclosure.updates.pbtxt",
            "//configs:omts/schunk.updates.pbtxt",
            "//configs:omts/camera_mount.updates.pbtxt",
            "//configs:omts/orbbec_gemini.updates.pbtxt",
        ],
    }),
)
```

*The ObjectWorldUpdates are grouped by robot cell: `select()` picks the `:is_lab_bb_01` list when you deploy with `--config=lab_bb_01`, and the `//conditions:default` list otherwise. A label like "//configs:lab_bb_01/scene.updates.pbtxt" refers to the file configs/lab\_bb\_01/scene.updates.pbtxt: the part before the colon is the directory containing the BUILD file (the "package"), and the part after it is the path of the file within that package.*

4. You're going to insert this line at the end of the `:is_lab_bb_01` list, which tells Bazel that your config should be part of the solution, and should override all previous configs. The ":relocate\_robot.updates.pbtxt" syntax tells Bazel that the file is in the same directory as the BUILD file you're editing, also known as the package.


```python
":relocate_robot.updates.pbtxt",
```


5. The result should look like this. Make sure you've saved your changes.


```python
# AFTER
intrinsic_solution(
    name = "omts_solution",
    assets = [
        [snip: lots of Assets]
    ],
    default_operation_mode = "real",
    instances = [
        [snip: lots of instances]
    ],
    object_world_updates = select({
        ":is_lab_bb_01": [
            "//configs:lab_bb_01/ur_module.attachments.updates.pbtxt",
            "//configs:lab_bb_01/scene.updates.pbtxt",
            "//configs:lab_bb_01/align_robot.updates.pbtxt",
            "//configs:lab_bb_01/orbbec_gemini.updates.pbtxt",
            ":relocate_robot.updates.pbtxt",
        ],
        "//conditions:default": [
            [snip: configurations that don't affect the lab_bb_01 config]
        ],
    }),
)
```

6. Rebuild the Solution with your changes and deploy it. This will update the "initial world" of the World Service to include your update.


```bash
cd ~/intrinsic-omts
bazel run //:omts_solution --config=lab_bb_01 -- \
  --address localhost:17080 --operation_mode=sim
```

7. Reset the world and simulator state to apply the configuration:


```bash
inctl world reset --address localhost:17080
```

8. Check RViz or Gazebo and you should see that the robot has moved:  

   ![RViz robot relocated permanently](../../img/learn/tutorials/rviz_robot_relocated_perm.png)

9. Check the state of the world. You should see that even after a world reset, the flange pose is different, because it starts from a new position now.


```bash
cd ~/intrinsic-omts
bazel --quiet run //tools/world:inspect_world -- --address localhost:17080
```

10. Clean up your changes so that they don't interfere with other tutorials:


```bash
rm relocate_robot.updates.pbtxt
git checkout BUILD
bazel run //:omts_solution --config=lab_bb_01 -- \
  --address localhost:17080 --operation_mode=sim
inctl world reset --address localhost:17080
```

## Architecture Overview

This diagram shows how the pose gets from the file you created to the different visualization UIs. The Intrinsic ROS bridge uses the same gRPC API as inspect\_world.

![Cell Customization Architecture Diagram](../../img/learn/tutorials/cell_customization_architecture.png)

## Troubleshooting

If you're having trouble with the visualization, check [Visualization Troubleshooting](visualize_the_robot.md#troubleshooting).


If you see this error when redeploying the solution:


```text
connection error: desc = "transport: error while dialing: dial unix /run/containerd/containerd.sock: connect: connection refused"
```

Run:


```bash
k9s -n app-intrinsic-base -c pods
```

Select the line "artifacts-deployment-...", press Ctrl+D to delete the [pod](../glossary/general_terms.md#pod), and select Enter, then retry.

## Next steps

It's time for a larger change: You'll replace the robot with one from a different manufacturer by [swapping assets](swap_assets.md).

