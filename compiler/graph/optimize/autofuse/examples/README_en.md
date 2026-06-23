# On-Device Verification

Assuming users have correctly set up the environment on their Ascend device, installed the matching version of CANN software packages, driver firmware packages, and built the TensorFlow environment. Here we demonstrate how to create corresponding Python scripts, set environment variables, run script test cases, and perform performance and accuracy analysis based on TensorFlow 1.15 networks.

## Script Creation

Currently, auto-fusion supports fusion of element+element, element+broadcast, element+reduce, and element+concat operator types.

Some complex computational operators, such as zerosLike, biasAdd, Squeeze, etc., are also currently supported for fusion. Fusion capabilities for more vector-type operators are being gradually released.

### Type 1: elementwise + elementwise

If you want to verify auto-fusion scenarios for several elementWise-type operators, you can refer to the Python files in the eleandele directory to construct and compile the corresponding models.

### Type 2: elementwise + broadcast

If you want to verify auto-fusion scenarios for elementWise-type and broadcast-type operators, you can refer to the Python files in the eleandbroadcast directory to construct and compile the corresponding models.

### Type 3: elementwise + reduce

If you want to verify auto-fusion scenarios for elementWise-type and reduce-type operators, you can refer to the Python files in the eleandreduce directory to construct and compile the corresponding models.

## Other Types (including operators like concat, split, slice, gather, transpose, etc.)

To be constructed.

## Running Test Cases

1. Set environment variables

   Before running test cases, you need to set the following environment variables, configure the NPU device to run on, and enable the auto-fusion feature.

   ```bash
    # Path to your own driver package installation
      source /usr/local/Ascend/driver/bin/setenv.sh
      # Path to your own CANN package installation, refer to (docs/build.md) to install the community toolkit package
      source /usr/local/Ascend/ascend-toolkit/set_env.sh
    
    export PYTHONPATH=/usr_path/:$PYTHONPATH
    # Assume running on device0
      export ASCEND_DEVICE_ID=0
      # Enable auto-fusion. --autofuse_enable_pass indicates additional fusion capabilities you want to enable, currently supports reduce, concat, slice, split, gather, transpose
      export AUTOFUSE_FLAGS="--enable_autofuse=true;--autofuse_enable_pass=reduce,concat,slice,split,gather,transpose;"
    It is generally recommended to only enable reduce and concat for the first test. Other data movement operators depend on the chip architecture and network structure to decide whether to enable them.

   ```

2. Run test cases
   python3 test.py

## Result Analysis

After enabling the auto-fusion feature, users often want to intuitively see which operators are included in the fused graph structure and which original operator nodes are contained within each fused operator. Developers can use CANN's dump graph capability to generate various dump graphs during the graph compilation phase in the set dump graph path through the following environment variable configuration.

   ```bash
    export DUMP_GE_GRAPH=2
    export DUMP_GRAPH_LEVEL=1
    export DUMP_GRAPH_PATH="/xxx/xxx/"
   ```

Users can analyze accuracy issues by enabling dump-related configurations. They can observe performance gains after enabling auto-fusion through profiling-related configurations. For detailed usage of accuracy debugging tools and Profiling performance analysis tools, please refer to [Accuracy Debugging Tool Guide](https://hiascend.com/document/redirect/CannCommunityToolAccucacy) and [Profiling Performance Analysis Tool Guide](https://hiascend.com/document/redirect/CannCommunityToolProfiling).
