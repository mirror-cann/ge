# Sample Usage Guide

## Function Description

This sample uses the BatchMatMulV2 flatten fusion pass as an example to introduce the implementation of a fusion pass that flattens BatchMatMul to MatMul, providing ATC tool offline model compilation verification. The pass is implemented using eager style API and fusion interfaces.

Fusion Principle: Flattens the BatchMatMulV2 operation of A[b,m,k] @ B[k,n] to a MatMulV2 operation of [b*m,k]@[k,n] through Reshape, then restores to [b,m,n] output shape through Reshape.

This sample only covers the educational scenario where A is 3-dimensional, B is 2-dimensional, both inputs have the same dtype of float/float16/bfloat16, no bias/offset_w, offset_x=0, and transpose attributes are all false.
BatchMatMul/BatchMatMulV2 with batch dimension broadcasting, bias, offset_w, or non-zero offset_x are not within the scope of this sample optimization.

## Directory Structure

```tree
├── README.md                                        // C++ sample documentation
├── src
│   ├──batch_matmul_flatten_pass.cpp                 // Pass implementation file
├── CMakeLists.txt                                   // Build script
├── data
│   ├──gen_onnx.py                                   // ONNX export script for ATC offline verification (supports --batch/--m/--k/--n parameters)
│   ├──quick_verify.sh                               // One-click verification script (supports shape and execution count parameters)
│   ├──benchmark_model.cpp                           // Performance test program source code
├── gen_es_api
│   ├──CMakeLists.txt                                // Build script for generating eager style API
```

## Environment Requirements

- Compiler: GCC >= 7.3.x
- Python and its dependency library versions: python>=3.9, onnx
- [Environment Preparation](../../../../../docs/en/build.md#1-environment-preparation) completed.

## Implementation Steps

1. Define class `BatchMatmulFlattenPass` inheriting from `PatternFusionPass`.
2. Override 3 functions from base class `PatternFusionPass`:
   - `Patterns` defines matching templates to capture topologies matching the template in the entire graph.
     - `pattern->CaptureTensor()` captures BatchMatMul nodes for reading attributes and input shapes.
   - `MeetRequirements` filters the matched topologies.
     - Checks that only x1/x2 valid inputs exist, input A is 3-dimensional, input B is 2-dimensional, both inputs have same floating-point dtype, offset_x=0, transpose attributes are false.
   - `Replacement` defines the replacement part.
     - Constructs Reshape+MatMulV2+Reshape replacement graph, choosing Const or dynamically computed reshape target shape based on whether shape is dynamic.
     - Uses `InferShapeAndCheckSupport` to verify replacement graph correctness.
3. Register `BatchMatmulFlattenPass` as a custom fusion pass with execution phase AfterInferShape.

## Program Compilation

Assume the CANN software package installation directory is INSTALL_PATH, for example `/home/HwHiAiUser/Ascend/`.

1. Configure environment variables.

   Run the environment variable script in the software package with the following command:

   ```bash
   source ${ASCEND_PATH}/set_env.sh
   ```

   `${ASCEND_PATH}` is the cann path under the CANN software package installation directory. Replace with the actual installation path of the relevant software package, for example `${INSTALL_PATH}/cann`.

2. Modify the following information in the `CMakeLists.txt` file in the current directory according to actual conditions.

   - ASCEND_PATH: You can set the default software package path. If `$ASCEND_HOME_PATH` is set via `set_env.sh`, no modification needed.

   - PASS_SO_DIR: You can set the custom fusion pass dynamic library installation directory name, default is `pass_so_dir`.

   - target_include_directories: Header files to include. For this sample, no modification needed. For user-developed code, add header files directly below the example when needed. Note: do not delete existing entries. If the network has custom operators, add custom operator prototype definition header files.

   - target_link_libraries: Libraries to link. For this sample, no modification needed. For user-developed code, add link libraries directly below the example when needed. Note: do not delete existing entries.

     > Do not link other so files in the software package, otherwise compatibility issues may occur during future upgrades.

3. Execute sequentially:

   ```bash
   mkdir build && cd build
   cmake ..
   ```

   After execution, the es_all_build/generated_code directory generated in the **build** directory contains header files and source code for ES graph building API.

4. Execute `make` command to compile the custom pass so. After successful compilation, install the dynamic library file `libbatch_matmul_flatten_pass.so` to the custom fusion pass directory via `make install`.
   You can add optional parameter `-j$(nproc)` after `make` for parallel build tasks. `$(nproc)` dynamically gets the CPU core count.

   ```bash
   make -j$(nproc) batch_matmul_flatten_pass
   make install
   ```

5. During compilation, the es so that the pass so depends on is generated in the build directory (located at `build/es_output/lib64`, named `libes_all.so`). `make install` only installs the pass so; the es so remains in the build directory. The runtime lookup path is already configured via `$ORIGIN` and the build directory path in CMakeLists.txt:
   - If the build directory remains in place, the pass so can find the es so directly via the build path at runtime, and no extra action is required.
   - If the build directory is deleted or the pass so is relocated (the original build path is no longer accessible at runtime), copy the es so to the pass so installation directory (i.e., `${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes`) so that it resides in the same directory as the pass so. It is then loaded from the same directory via `$ORIGIN` at runtime, without setting `LD_LIBRARY_PATH`.

   ```bash
   cp build/es_output/lib64/libes_all.so ${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes/
   ```

   After sample verification is complete, execute the following command to clean the custom pass so installed in the CANN package to avoid affecting subsequent UT/ST:

   ```bash
   make clean_custom_pass
   ```

## Program Execution

1. Configure environment variables (skip if already executed).

   - Run the environment variable script in the software package with the following command:

     ```bash
     source ${ASCEND_PATH}/set_env.sh
     ```

     Replace `${ASCEND_PATH}` with the actual installation path of the relevant software package.

2. Use ATC for offline inference.

   - Set environment variables to dump the model graph during compilation:

     ```bash
     export DUMP_GE_GRAPH=1
     ```

   - Navigate to the `data` directory in the current directory and execute the `.py` file to export onnx (the file uses the onnx library, ensure it's installed before running):

     ```python
     python gen_onnx.py
     ```

   - You can also specify shape parameters to export models with different shapes:

     ```python
     python gen_onnx.py --batch 32 --m 64 --k 512 --n 256
     ```

   - After execution, a `.onnx` format model file named `model.onnx` is generated in the `data` directory.
   - Execute the ATC tool command (for detailed ATC tool instructions, visit [Ascend Documentation](https://www.hiascend.com/zh/document) and search for "ATC Offline Model Compilation Tool"), replace `soc_version` according to your actual environment:

     ```bash
     atc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model_fused
     ```

   - The following output appears in the logs:

     ```text
     Define pattern for BatchMatmulFlattenPass
     Define MeetRequirements for BatchMatmulFlattenPass
     Define replacement for BatchMatmulFlattenPass
     Created node: Reshape
     Created node: MatMulV2
     Created node: Reshape
     InferShapeAndCheckSupport success
     ```

3. One-click Verification (Optional)

   - Use the `quick_verify.sh` script to complete compilation, ATC, dump graph inspection, and performance testing in one click. The `soc_version` in the script defaults to `Ascend910B3`, modify the `--soc_version` parameter in the atc command in the script according to your actual environment (refer to [Using ATC for Offline Inference](#program-compilation)):

     ```bash
     cd data
     ./quick_verify.sh [batch] [m] [k] [n] [test_rounds]
     ```

   - Default parameters: batch=32, m=64, k=512, n=256, test_rounds=3

   - The script automatically:
     1. Checks and compiles Pass (if not compiled)
     2. Checks and compiles benchmark_model
     3. Generates ONNX model
     4. ATC compiles fusion model
     5. Checks dump graph to verify fusion effect
     6. Runs multiple rounds of performance testing
     7. Cleans custom pass so installed in CANN package

   - If the pre-fusion dump graph has no BatchMatMul node, or the post-fusion graph doesn't have the Reshape+MatMulV2+Reshape replacement structure, the script will exit with failure.

4. View Results

   - After the ATC tool command completes, a series of `.pbtxt` and `.txt` files are generated in the directory.
     Compare the following dump graphs:
     - `ge_proto_xxxxx_graph_x_PreRunBegin.txt` pre-execution dump graph, should contain BatchMatMulV2 node
     - `ge_proto_xxxxx_graph_x_RunCustomPass_AfterInferShape.txt` custom pass dump graph after InferShape, should contain Reshape+MatMulV2+Reshape nodes, no longer contains BatchMatMulV2 node

     You can see the model has been optimized as expected, i.e., BatchMatMulV2 is replaced by Reshape+MatMulV2+Reshape.

   - If expected results are not obtained, you can set the following environment variables (if using atc command, also add parameter `--log=debug`) to print logs to screen for troubleshooting:

     ```bash
     export ASCEND_SLOG_PRINT_TO_STDOUT=1
     export ASCEND_GLOBAL_LOG_LEVEL=0
     ```
