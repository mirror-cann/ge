# Sample Usage Guide

## Feature Description

This sample demonstrates a custom pass for custom operator AddCustom. **This use case targets scenarios where users can access custom operator prototypes**
The pass implementation in this example: for AddCustom with one input being 0, perform deletion operation.
Provides two verification methods: online inference and ATC offline model compilation. The sample uses eager style API and fusion interface.

## Directory Structure

```tree
├── README.md                     // C++ sample documentation
├── src
│   ├──addcustom_zero_pass.cpp   // pass implementation file
├── CMakeLists.txt               // build script
├── data
|   ├──torch_forward.py          // torch script for online inference
|—— gen_es_api
|   |——CMakeLists.txt            // build script for generating eager style api
|—— proto                        // custom operator prototypes
|   |——add_custom_proto.cc
|   |——add_custom_proto.h
```

## Environment Requirements

- Compiler: GCC >= 7.3.x
- Python and dependencies: python>=3.9, pytorch>=2.1
- [Environment preparation](../../../../../docs/zh/build.md) completed.

## Preparation

1. Create custom operator project: prerequisite for writing custom pass for custom operators is user has created custom operator project, refer to [Custom Operator Graph Integration](https://www.hiascend.com/document/detail/zh/Pytorch/730/modthirdparty/torchairuseguide/torchair_00055.html). During this phase, users need to complete: custom operator implementation, custom operator package compilation and deployment, custom operator adaptation development.
2. Obtain operator prototype: After successful compilation, copy custom operator prototype definitions from build_out/autogen path in custom operator project to proto directory in current project. This sample already has AddCustom custom operator prototype in proto directory, users can replace or add as needed.

## Program Compilation

1. Configure environment variables.

   - Run environment setup script:

     ```bash
     source ${ASCEND_PATH}/set_env.sh
     ```

     `${ASCEND_PATH}` is cann path under CANN software package installation directory. Replace with actual installation path, e.g., `${INSTALL_PATH}/cann`.

2. Modify **gen_es_api/CMakeLists** file as needed:
    - Modify custom operator prototype file path: add_library(custom_op_proto SHARED .../proto/your_proto_name.cc)

3. Modify **CMakeLists.txt** as needed.

- ASCEND_PATH: Default software package path. If `$ASCEND_HOME_PATH` set via set_env.sh, no modification needed.

- PASS_SO_DIR: Custom fusion pass dynamic library installation directory name, default `pass_so_dir`.

- target_include_directories: Required header files. For this sample, no modification needed. For custom development, add header files below the example without deleting existing items. If network has custom operators, add custom operator prototype definition headers.

- target_link_libraries: Required libraries. For this sample, no modification needed. For custom development, add libraries below the example without deleting existing items.

  > Do not link other SOs from software package to avoid compatibility issues during future upgrades.

1. Execute sequentially:

   ```bash
   mkdir build && cd build
   cmake ..
   ```

2. After pass writing completed, run following commands to compile custom pass so and copy compiled dynamic library libadd_zero_pass.so to custom fusion pass directory, where "xxx" is user-defined directory.
   Optional parameter `-j$(nproc)` can be added after make for parallel build tasks, `$(nproc)` dynamically gets CPU core count.

   ```bash
   make -j$(nproc) add_custom_zero_pass
   make install
   ```

6. During compilation, the es so that the pass so depends on is generated in the build directory (located at `build/es_output/lib64`, named `libes_all.so` and `libes_custom.so`). `make install` only installs the pass so; the es so remains in the build directory. The runtime lookup path is already configured via `$ORIGIN` and the build directory path in CMakeLists.txt:
   - If the build directory remains in place, the pass so can find the es so directly via the build path at runtime, and no extra action is required.
   - If the build directory is deleted or the pass so is relocated (the original build path is no longer accessible at runtime), copy the es so to the pass so installation directory (i.e., `${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes`) so that it resides in the same directory as the pass so. It is then loaded from the same directory via `$ORIGIN` at runtime, without setting `LD_LIBRARY_PATH`.

   ```bash
   cp build/es_output/lib64/libes_all.so build/es_output/lib64/libes_custom.so ${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes/
   ```

   After sample verification, run the following command to clean custom pass so installed under CANN package to avoid affecting subsequent UT/ST:

   ```bash
   make clean_custom_pass
   ```

## Pass Writing

1. Define class `AddCustomZeroPass` inheriting from `PatternFusionPass`.
2. Override three functions from base class `PatternFusionPass`:
   - `Patterns` defines matching templates for identifying topologies matching the template in the graph.
   - `MeetRequirements` filters topologies matched by template.
   - `Replacement` defines replacement part.
3. Register `AddCustomZeroPass` as custom fusion pass with execution phase BeforeInferShape.

## Verification

1. Configure environment variables.

    - Run environment setup script:

      ```bash
      source ${ASCEND_PATH}/set_env.sh
      ```

      `${ASCEND_PATH}` is cann path under CANN software package installation directory. Replace with actual installation path, e.g., `${INSTALL_PATH}/cann`.

2. Online inference
   - Set environment variable to dump model graph during compilation:

      ```bash
      export DUMP_GE_GRAPH=1
      ```

   - Enter data directory and execute .py file for online inference:

      ```python
      python torch_forward.py
      ```

   - Log shows:

     ```text
     Define pattern for AddCustomZeroPass
     Define MeetRequirements for AddCustomZeroPass
     Define replacement for AddCustomZeroPass
     ```

3. View results

   - After execution, series of .pdtxt files generated in directory.
      Compare the following dump graphs:
     - `ge_onnx_xxxxx_PreRunBegin.pdtxt` dump graph before execution
     - `ge_onnx_xxxxx_RunCustomPassBeforeInferShape.pdtxt` custom pass dump graph before InferShape execution

      Model optimized as expected, i.e., add-zero nodes removed.

   - If results not as expected, set following environment variables (for atc command, also add parameter `--log=debug`) to print logs to screen for troubleshooting.

     ```bash
      export ASCEND_SLOG_PRINT_TO_STDOUT=1 #print logs to screen
      export ASCEND_GLOBAL_LOG_LEVEL=0 #log level debug
     ```
