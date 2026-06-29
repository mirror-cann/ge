# Sample Usage Guide<a name="ZH-CN_TOPIC_0345664697"></a>

## Feature Description<a name="section5991635456363"></a>

This sample demonstrates capture tensor functionality using MatMul+Add fusion to GEMM pass, providing two verification methods: online inference and ATC offline model compilation. The pass uses eager style API and fusion interface.

## Directory Structure<a name="section7668345634665"></a>

```tree
├── src
│   ├──fuse_matmul_add_pass.cpp  // pass implementation file
├── CMakeLists.txt               // build script
├── data
|   ├──torch_gen_onnx.py         // torch script for exporting onnx
|   ├──torch_forward_1.py          // torch script for online inference, pass executed successfully
|   ├──torch_forward_2.py          // torch script for online inference, pass blocked
|—— gen_es_api
|   |——CMakeLists.txt            // build script for generating eager style api
```

## Environment Requirements<a name="section383335652346"></a>

- Compiler: GCC >= 7.3.x
- Python and dependencies: python>=3.9, pytorch>=2.1
- [Environment preparation](../../../../../docs/zh/build.md) completed.

## Implementation Steps

1. Define class `FuseMatMulAndAddPass` inheriting from `PatternFusionPass`.
2. Override three functions from base class `PatternFusionPass`:
    - `Patterns` defines matching templates for identifying topologies matching the template in the graph.
      - `pattern->CaptureTensor()` captures tensor, tensor structure: `{NodeIo,index}`.
    - `MeetRequirements` filters topologies matched by template.
      - `match_result->GetCapturedTensor(kAddCaptureIdx,add_node);` reads captured NodeIo for checking.
    - `Replacement` defines replacement part.
      - `match_result->GetCapturedTensor(kMatMulCaptureIdx, matmul_node);` reads captured NodeIo to extract attribute values.
3. Register `FuseMatMulAndAddPass` as custom fusion pass with execution phase BeforeInferShape.

## Program Compilation<a name="section6645633456813"></a>

1. Configure environment variables.

   Run environment setup script from software package:

   ```bash
   source ${ASCEND_PATH}/set_env.sh
   ```

   `${ASCEND_PATH}` is cann path under CANN software package installation directory. Replace with actual installation path, e.g., `${INSTALL_PATH}/cann`.

2. Modify **CMakeLists.txt** as needed.

   - ASCEND_PATH: Default software package path. If `$ASCEND_HOME_PATH` set via set_env.sh, no modification needed.

   - target_include_directories: Required header files. For this sample, no modification needed. For custom development, add header files below the example without deleting existing items. If network has custom operators, add custom operator prototype definition headers.

   - target_link_libraries: Required libraries. For this sample, no modification needed. For custom development, add libraries below the example without deleting existing items.

      > Do not link other SOs from software package to avoid compatibility issues during future upgrades.

3. Execute sequentially:

   ```bash
   mkdir build && cd build
   cmake ..
   ```

4. Run make to compile custom pass so, then install dynamic library   libfuse_matmul_add_for_capture_tensor_sample_pass.so to custom fusion pass directory via make install.
   Optional parameter `-j$(nproc)` can be added after make for parallel build tasks, `$(nproc)` dynamically gets CPU core count.

   ```bash
   make -j$(nproc) fuse_matmul_add_for_capture_tensor_sample_pass
   make install
    ```

5. During compilation, the es so that the pass so depends on is generated in the build directory (located at `build/es_output/lib64`, named `libes_all.so`). `make install` only installs the pass so; the es so remains in the build directory. The runtime lookup path is already configured via `$ORIGIN` and the build directory path in CMakeLists.txt:
   - If the build directory remains in place, the pass so can find the es so directly via the build path at runtime, and no extra action is required.
   - If the build directory is deleted or the pass so is relocated (the original build path is no longer accessible at runtime), copy the es so to the pass so installation directory (i.e., `${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes`) so that it resides in the same directory as the pass so. It is then loaded from the same directory via `$ORIGIN` at runtime, without setting `LD_LIBRARY_PATH`.

   ```bash
   cp build/es_output/lib64/libes_all.so ${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes/
   ```

   After sample verification, run the following command to clean custom pass so installed under CANN package to avoid affecting subsequent UT/ST:

   ```bash
   make clean_custom_pass
   ```

## Program Execution<a name="section4524573456563512"></a>

1. Configure environment variables (if already done, skip).

    - Run environment setup script:

      ```bash
      source ${ASCEND_PATH}/set_env.sh
      ```

      Replace `${ASCEND_PATH}` with actual software package installation path.

2. Use ATC offline inference.

    - Set environment variable to dump model graph during compilation:

      ```bash
      export DUMP_GE_GRAPH=1
      ```

    - Enter data directory and execute .py file to export onnx (uses torch onnx exporter, depends on additional Python package onnx, ensure installed before running. ATC tool currently supports onnx opset_version up to 18, if torch exports higher version by default, specify explicitly, see script comments):

      ```python
      python torch_gen_onnx.py
      ```

    - After execution, .onnx format model file named model.onnx generated in data directory.
    - Execute ATC tool command (for detailed ATC tool instructions, visit [Ascend Documentation](https://www.hiascend.com/zh/document) and search for "ATC Offline Model Compilation Tool"), modify `soc_version` based on actual environment:

      ```bash
      atc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model
      ```

    - Log shows:

      ```text
      Define pattern for FuseMatMulAndAddPass in capture tensor sample
      Define MeetRequirements for FuseMatMulAndAddPass in capture tensor sample
      Define replacement for FuseMatMulAndAddPass in capture tensor sample
      ```

3. Online inference
    - Set environment variable to dump model graph during compilation:

       ```bash
       export DUMP_GE_GRAPH=1
       ```

    - Enter data directory and execute .py file for online inference (ensure torch_npu plugin installed for online inference), execute `torch_forward_1.py`:

       ```python
       python torch_forward_1.py
       ```

    - For torch_forward_1.py, log shows:

      ```text
       Define pattern for FuseMatMulAndAddPass in capture tensor sample
      Define MeetRequirements for FuseMatMulAndAddPass in capture tensor sample
       Define replacement for FuseMatMulAndAddPass in capture tensor sample
      ```

    - Execute `torch_forward_2.py`:

      ```python
      python torch_forward_2.py
      ```

    - For torch_forward_2.py, log shows:

      ```text
      Define pattern for FuseMatMulAndAddPass in capture tensor sample
      Define MeetRequirements for FuseMatMulAndAddPass in capture tensor sample
      Only support Add inputs are fp32
      ```

4. View results

    - After execution, series of .pbtxt files generated in directory.
      Compare the following dump graphs:
        - `ge_onnx_xxxxx_PreRunBegin.pbtxt` dump graph before execution
        - `ge_onnx_xxxxx_RunCustomPassBeforeInferShape.pbtxt` custom pass dump graph before InferShape execution

      Model optimized as expected, i.e., MatMul and Add replaced by GEMM.

    - If results not as expected, set following environment variables (for atc command, also add parameter `--log=debug`) to print logs to screen for troubleshooting.

     ```bash
      export ASCEND_SLOG_PRINT_TO_STDOUT=1 #print logs to screen
      export ASCEND_GLOBAL_LOG_LEVEL=0 #log level debug
     ```
