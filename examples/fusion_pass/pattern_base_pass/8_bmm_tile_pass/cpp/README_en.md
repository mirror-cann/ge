# Sample Usage Guide

## Function Description

This sample uses the BmmTile fusion pass as an example to introduce how to use the PatternFusionPass framework to eliminate redundant Tile operators in "shared weight + batch input" scenarios in the computation graph, leveraging BatchMatMulV2's native batch broadcasting capability instead of explicit data expansion, providing ATC tool offline model compilation verification.

Fusion Principle: When the weight shape's first dimension is 1 (e.g., `[1, k, n]`), after being expanded by Tile to `[batch, k, n]` and then performing BatchMatMulV2 with input `[batch, m, k]`, since BatchMatMulV2 supports first-dimension broadcasting, the Tile can be directly removed and `[1, k, n]` can be directly connected to BatchMatMulV2, with BMM automatically broadcasting to complete the computation.

This sample covers two types of equivalent patterns:

- Pattern 1: `Tile -> BatchMatMulV2`, remove Tile, connect Tile input directly to BMM.
- Pattern 2: `Tile -> Reshape -> Transpose -> BatchMatMulV2`, remove Tile, connect Tile input directly to Reshape; Reshape target shape first dimension changed to 0 (copy from input), Reshape/Transpose/BMM first dimension becomes 1.

Each pattern may have Tile on either the input0 or input1 side of BMM, resulting in 4 patterns total.

## Directory Structure

```bash
├── README.md                                        // C++ sample documentation
├── src
│   ├──bmm_tile_pass.cpp                             // Pass implementation file
├── CMakeLists.txt                                   // Build script
├── data
│   ├──gen_onnx.py                                   // ONNX export script for ATC offline verification (supports --batch/--m/--k/--n parameters)
│   ├──quick_verify.sh                               // One-click verification script (supports shape and execution count parameters)
├── gen_es_api
│   ├──CMakeLists.txt                                // Build script for generating eager style API
```

## Environment Requirements

- Compiler: GCC >= 7.3.x
- Python and its dependency library versions: python>=3.9, onnx, numpy
- [Environment Preparation](../../../../../docs/zh/build.md#1-环境准备) completed.

## Implementation Steps

1. Define class `BmmTilePass` inheriting from `PatternFusionPass`.
2. Override 3 functions from base class `PatternFusionPass`:
   - `Patterns` defines 4 matching templates, covering combinations of Tile on BMM input0/input1 side and whether through Reshape->Transpose.
     - Pattern 1a/1b: `Tile -> BMM`, Tile on input0 and input1 side respectively.
     - Pattern 2a/2b: `Tile -> Reshape -> Transpose -> BMM`, Tile on input0 and input1 side respectively.
     - Each pattern captures Tile and BMM nodes via `CaptureTensor`; Pattern 2 additionally captures Reshape and Transpose nodes.
   - `MeetRequirements` filters the matched topologies.
     - Common checks: Tile output has only 1 consumer; Tile input dimension 0 == 1 and BMM other side dimension 0 != 1; Tile multiples only has non-1 in first dimension.
     - Pattern 2 additional checks: Transpose perm is Const and `perm[0] == 0`; Reshape shape is Const; Tile multiples is Const and `multiples[0] == Reshape first dimension`, other dimensions == 1; Reshape/Transpose each have only 1 consumer.
   - `Replacement` defines the replacement part.
     - Pattern 1: directly `BatchMatMulV2(tile_input, bmm_other)`, no Tile.
     - Pattern 2: `Reshape(tile_input, new_shape) -> Transpose -> BMM`, new_shape first dimension changed to 0 (copy from input, equivalent to first dimension being 1).
     - Uses `InferShapeUtil::InferShape` to infer replacement graph shape.
3. Register `BmmTilePass` as a custom fusion pass with execution phase AfterInferShape.

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

4. Execute `make` command to compile the custom pass so. After successful compilation, install the dynamic library file `libbmm_tile_pass.so` to the custom fusion pass directory via `make install`.
   You can add optional parameter `-j$(nproc)` after `make` for parallel build tasks. `$(nproc)` dynamically gets the CPU core count.

```bash
   make -j$(nproc) bmm_tile_pass
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

```bash
     python gen_onnx.py
```

   - You can also specify shape parameters to export models with different shapes:

```bash
     python gen_onnx.py --batch 4 --m 128 --k 256 --n 512
```

   - After execution, a `.onnx` format model file named `model.onnx` is generated in the `data` directory.
   - Execute the ATC tool command (for detailed ATC tool instructions, visit [Ascend Documentation](https://www.hiascend.com/zh/document) and search for "ATC Offline Model Compilation Tool"), replace `soc_version` according to your actual environment:

```bash
     atc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model_fused
```

   - The following output appears in the logs:

```text
     Define pattern for BmmTilePass
     Define MeetRequirements for BmmTilePass
     Define replacement for BmmTilePass
     Created node: BatchMatMulV2
     InferShape success
```

3. One-click Verification (Optional)

   - Use the `quick_verify.sh` script to complete compilation, ATC, dump graph inspection, and performance testing in one click. The `soc_version` in the script defaults to `Ascend910B3`, modify the `--soc_version` parameter in the atc command in the script according to your actual environment (refer to [Using ATC for Offline Inference](#program-execution)):

```bash
     cd data
     ./quick_verify.sh [batch] [m] [k] [n] [test_rounds]
```

   - Default parameters: batch=2, m=64, k=128, n=256, test_rounds=3

   - The script automatically:
     1. Checks and compiles Pass (if not compiled)
     2. Generates ONNX model
     3. ATC compiles fusion model
     4. Checks dump graph to verify fusion effect
     5. Runs multiple rounds of performance testing (if benchmark_model available)
     6. Cleans custom pass so installed in CANN package

   - If the pre-fusion dump graph has no Tile node, or the post-fusion Tile node is not removed, the script will exit with failure.

4. View Results

   - After the ATC tool command completes, a series of `.pbtxt` and `.txt` files are generated in the directory.
     Compare the following dump graphs:
     - `ge_proto_xxxxx_graph_x_PreRunBegin.txt` pre-execution dump graph, should contain Tile and BatchMatMulV2 nodes
     - `ge_proto_xxxxx_graph_x_RunCustomPass_AfterInferShape.txt` custom pass dump graph after InferShape, should no longer contain Tile node, BatchMatMulV2 retained

     You can see the model has been optimized as expected, i.e., redundant Tile is removed and BatchMatMulV2 uses first-dimension broadcasting to complete computation.

   - If expected results are not obtained, you can set the following environment variables (if using atc command, also add parameter `--log=debug`) to print logs to screen for troubleshooting:

     ```bash
     export ASCEND_SLOG_PRINT_TO_STDOUT=1
     export ASCEND_GLOBAL_LOG_LEVEL=0
     ```
