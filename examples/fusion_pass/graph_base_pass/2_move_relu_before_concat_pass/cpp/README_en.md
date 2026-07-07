# Example Usage Guide

## Function Description

This example is a custom pass for moving ReLU before Concat. When fusion pass scenarios involve **operators with dynamic input/output counts**, refer to this example.
The example provides both online inference and atc tool offline model compilation to demonstrate how the framework calls custom pass to complete graph optimization,
using eager style api and fusion interfaces.

## Directory Structure

```tree
├── src
│   ├──move_relu_before_concat_pass.cpp                 // pass implementation file
├── CMakeLists.txt                                      // build script
├── data
|   ├──es_gen_air.py                                // export air
|   ├──torch_forward.py                                 // torch script for online inference
|—— gen_es_api
|   |——CMakeLists.txt                                   // build script for generating eager style api
```

## Environment Requirements

- Compiler: GCC >= 7.3.x
- Python and dependency versions: python>=3.9, pytorch>=2.1
- Completed [environment preparation](../../../../../docs/zh/build.md).

## Implementation Steps

1. Define `MoveReluBeforeConcatPass` class inheriting `FusionBasePass`.
2. Override base class `FusionBasePass` `Run` method, implement custom pass logic.
3. Define `FindConcatNodesMeetRequirements` to traverse nodes in graph, get Concat nodes meeting conditions.
4. Define `MoveReluBeforeConcat` to implement graph modification:
   - `Replacement` builds replacement structure based on concat node
   - `GetSubgraphBoundary` builds boundary of subgraph to be replaced
   - Finally call `SubgraphRewriter` `Replace` method to implement replacement

## Program Compilation

Assume CANN package installation directory is INSTALL_PATH, e.g., `/home/HwHiAiUser/Ascend/`.

1. Configure environment variables.

   Run environment variable script from the package:

   ```bash
   source ${ASCEND_PATH}/set_env.sh
   ```

   `${ASCEND_PATH}` is the cann path under CANN package installation directory. Replace with actual installation path, e.g., `${INSTALL_PATH}/cann`.

2. Modify the following information in **CMakeLists.txt** as needed.

    - ASCEND_PATH: Can set default package path. If `$ASCEND_HOME_PATH` is set via set_env.sh, no modification needed.

    - PASS_SO_DIR: Can set custom fusion pass dynamic library installation directory name, default is `pass_so_dir`.

    - target_include_directories: Header files to include. For this example, no modification needed. For user-developed code, when adding headers, add lines below the example, do not delete existing items. If network has custom operators, add custom operator prototype definition header files.

    - target_link_libraries: Libraries to link. For this example, no modification needed. For user-developed code, when adding link libraries, add lines below the example, do not delete existing items.

      > Do not link other so from the package, otherwise may cause compatibility issues during future upgrades.

3. Execute sequentially:

   ```bash
   mkdir build && cd build
   cmake ..
   ```

4. Execute make command to compile custom pass so. After successful compilation, use make install to install dynamic library file libmove_relu_before_concat_pass.so to custom fusion pass directory.
   Can add optional parameter `-j$(nproc)` after make for parallel build, `$(nproc)` dynamically gets CPU core count.

   ```bash
   make -j$(nproc) move_relu_before_concat_pass
   make install
   ```

5. During compilation, the es so that the pass so depends on is generated in the build directory (located at `build/es_output/lib64`, named `libes_all.so`). `make install` only installs the pass so; the es so remains in the build directory. The runtime lookup path is already configured via `$ORIGIN` and the build directory path in CMakeLists.txt:
   - If the build directory remains in place, the pass so can find the es so directly via the build path at runtime, and no extra action is required.
   - If the build directory is deleted or the pass so is relocated (the original build path is no longer accessible at runtime), copy the es so to the pass so installation directory (i.e., `${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes`) so that it resides in the same directory as the pass so. It is then loaded from the same directory via `$ORIGIN` at runtime, without setting `LD_LIBRARY_PATH`.

   ```bash
   cp build/es_output/lib64/libes_all.so ${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes/
   ```

   After example validation completes, execute the following command to clean custom pass so installed under CANN package, to avoid affecting subsequent UT/ST:

   ```bash
   make clean_custom_pass
   ```

## Program Execution

1. Configure environment variables (if already done, skip).

    - Run environment variable script from the package:

      ```bash
      source ${ASCEND_PATH}/set_env.sh
      ```

      Replace `${ASCEND_PATH}` with actual package installation path.

2. Use ATC offline inference.

    - Set environment variable to dump model graph during compilation:

      ```bash
      export DUMP_GE_GRAPH=1
      ```

    - Set the build directory environment variable. `BUILD_PATH` is the actual build directory generated in "Program Compilation":

      ```bash
      export BUILD_PATH=/path/to/build
      ```

    - Install es_all.whl

      ```bash
      pip install --force-reinstall --upgrade --target ${ASCEND_PATH}/python/site-packages/ ${BUILD_PATH}/es_output/whl/es_all-*.whl
      ```

    - Set environment variable to add es_all.so path

      ```bash
      export LD_LIBRARY_PATH="${BUILD_PATH}/es_output/lib64:${LD_LIBRARY_PATH}"
      ```

    - Enter data directory and execute .py file to export air:

      ```python
      python es_gen_air.py
      ```

    - After execution, .air format model file named graph.air is generated in data directory.
    - Execute ATC tool command (for detailed ATC tool instructions, visit [Ascend Documentation](https://www.hiascend.com/zh/document) and search "ATC Offline Model Compilation Tool"), modify `soc_version` per actual environment:

      ```bash
      atc --model=./graph.air --framework=1 --soc_version=xxx --output=./model
      ```

    - Following log appears:

      ```text
      MoveReluBeforeConcatPass
      Define Replacement for MoveReluBeforeConcatPass
      Replacement of MoveReluBeforeConcatPass succeeded
      ```

3. Online inference
    - Set environment variable to dump model graph during compilation:

       ```bash
       export DUMP_GE_GRAPH=1
       ```

    - Enter data directory and execute .py file for online inference (ensure torch_npu plugin is installed for online inference):

       ```python
       python torch_forward.py
       ```

    - Following log appears:

      ```text
      MoveReluBeforeConcatPass
      Define Replacement for MoveReluBeforeConcatPass
      Replacement of MoveReluBeforeConcatPass succeeded
      ```

4. View execution results

    - After ATC tool command completes, a series of .pbtxt files are generated in the directory.
      Compare the following dump graphs:
        - `ge_onnx_xxxxx_PreRunBegin.pbtxt` dump graph before execution
        - `ge_onnx_xxxxx_RunCustomPassBeforeInferShape.pbtxt` custom pass dump graph before InferShape execution

      Find model optimized as expected, i.e., ReLU moved before Concat.

    - If expected result is not obtained, can set the following environment variables (if using atc command, also add parameter `--log=debug`) to print logs to screen for troubleshooting:

      ```bash
        export ASCEND_SLOG_PRINT_TO_STDOUT=1 # print logs to screen
        export ASCEND_GLOBAL_LOG_LEVEL=0 # log level debug
      ```
