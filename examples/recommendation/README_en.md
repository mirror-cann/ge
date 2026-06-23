# Sample Usage Guide

## Function Description

This sample is a high-performance inference example for recommendation networks, demonstrating how to use **GE API** and **ACL API** to implement the inference workflow, and optimize inference throughput for recommendation networks through **multi-instance**, **batch H2D (Host-to-Device) memory copy**, and **AI Core control** technologies.

## Directory Structure

```tree
├── src
│   ├──model_inference.cpp               // Model inference implementation file
│   ├──model_inference.h                 // Model inference header file
│   ├──recomand_random_input.cpp         // Inference client, constructs random data, calls model inference
├── CMakeLists.txt                       // Build script
```

## Environment Requirements

- [Ascend AI Software Stack Deployment in Development Environment](../../docs/zh/quick_install.md) completed

## Implementation Steps

1. Graph Construction: Parse model files using aclgrphParseTensorFlow to build GE computation graph.
2. Graph Compilation and Loading: Compile (Compile) and load (Load) the graph through GE API (ge::Graph, ge::Session).
3. Data Preparation and Execution: Construct random data based on model input structure, use GE API for inference.
4. Performance Optimization:
   - Multi-instance: Create multiple inference instances through multi-threading to improve system concurrent processing capability.
   - Core control: When creating ge::Session, specify the number of AI Cores available for single operators through options parameter.
   - Batch H2D: Use aclrtMemcpyBatch interface to merge multiple memory copy operations to reduce overhead.

## Build and Verification

Assume toolkit installation directory is install_path, for example `/home/HwHiAiUser/Ascend/cann/`

1. Configure environment variables.

   ```bash
   source ${install_path}/set_env.sh
   ```

2. Execute the following command to create data directory, and [download](https://ascend-cann.obs.cn-north-4.myhuaweicloud.com/CANN/community/ge/DCN_v2.pb) model pb file and place it in data directory.

   ```shell
   mkdir data
   ```

3. Execute the following command to compile and generate executable file

   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

   After execution, recomand_exec executable file will be generated in **build** directory

4. Execute the following command to test recommendation network inference throughput without optimization features.

   ```shell
   ./recomand_exec
   ```

5. Test network inference performance with 4 multi-instances, batch H2D, and core control enabled, where aiCoreNum refers to [GE Graph Engine Interface -> Data Types -> options Parameter Description](https://www.hiascend.com/document/redirect/CannCommunityAscendGraphApi) and adjust according to actual hardware information.

   ```shell
   ./recomand_exec --multiInstanceNum=4 --enableBatchH2D=true --aiCoreNum="16|16"
   ```

## Performance Comparison

Throughput (TPS) and latency (ms) performance under different configurations on Ascend 910C platform:

 | Configuration                    | Throughput / Latency               |
 | --------------------------- |-----------------------|
 | Single instance                      | 745,55 TPS / 1.471ms  |
 | Single instance + Batch H2D             | 131,191 TPS / 0.792ms |
 | Multi-instance (4)                  | 155,104 TPS / 2.089ms |
 | Multi-instance (4) + Core control (16\|16)    | 185,415 TPS / 1.797ms |
 | Multi-instance (4) + Core control (16\|16) + Batch H2D | 251,877 TPS / 1.285ms |
