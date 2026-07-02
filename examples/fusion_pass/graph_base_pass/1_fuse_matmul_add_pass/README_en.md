# Example Usage Guide<a name="ZH-CN_TOPIC_0345664697"></a>

## Function Description<a name="section5991635456363"></a>

This example is a MatMul+Add fused to GEMM custom pass example, demonstrating how the framework calls custom pass to complete graph optimization using ATC offline inference and TF online inference.

## Directory Structure<a name="section7668345634665"></a>

```tree
├── src
│   ├──fuse_matmul_add_pass.cpp  // pass implementation file
├── CMakeLists.txt               // build script
├── data
│   ├──tensorflow_generate.py    // generate .pb format TensorFlow model for offline inference
|   ├──tf_forward.py             // TF online build original graph then do custom pass and other framework built-in pass optimization, then execute optimized graph to get result
```

## Environment Requirements<a name="section383335652346"></a>

- Compiler: GCC >= 7.3.x
- Python and dependency versions: python==3.7, TensorFlow==1.15.0, numpy==1.21.6
- Completed [environment preparation](../../../../docs/zh/build.md).

## Program Compilation<a name="section6645633456813"></a>

Assume CANN package installation directory is INSTALL_PATH, e.g., `/home/HwHiAiUser/Ascend/`.

1. Configure environment variables.

   Run the environment variable script from the package:

   ```bash
   source ${ASCEND_PATH}/set_env.sh
   ```

   `${ASCEND_PATH}` is the cann path under CANN package installation directory. Replace with actual installation path, e.g., `${INSTALL_PATH}/cann`.

2. Modify the following information in **CMakeLists.txt** as needed.

    - ASCEND_PATH: Can set default package path. If `$ASCEND_HOME_PATH` is set via set_env.sh, no modification needed.

    - FUSION_PASS_DIR: Can set custom fusion pass dynamic library installation directory name, default is `fusion_passes`.

    - target_include_directories: Header files to include. For this example, no modification needed. For user-developed code, when adding headers, add lines below the example, do not delete existing items. If network has custom operators, add custom operator prototype definition header files.

    - target_link_libraries: Libraries to link. For this example, no modification needed. For user-developed code, when adding link libraries, add lines below the example, do not delete existing items.

      > Do not link other so from the package, otherwise may cause compatibility issues during future upgrades.

3. Execute the following commands to compile. After compilation, dynamic library file **libfuse_matmul_add_pass.so** is generated in **build** directory.

   Execute sequentially:

   ```bash
   mkdir build && cd build
   cmake .. && make
   ```

4. After successful compilation, use make install to install dynamic library file libfuse_matmul_add_pass.so to custom fusion pass directory.

   ```bash
   make install
   ```

   After example validation completes, execute the following command to clean custom pass so installed under CANN package, to avoid affecting subsequent UT/ST:

   ```bash
   make clean_custom_pass
   ```

## Program Execution<a name="section4524573456563512"></a>

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

    - In **data** directory, execute TensorFlow original model generation script:

       **python tensorflow_generate.py**

       After execution, .pb format model file named **matmul_add.pb** is generated in **data** directory.

    - Execute ATC command, fill soc_version according to actual model runtime environment:

      **atc --model=./matmul_add.pb --framework=3 --soc_version=xxx --output=./matmul_add**

      After command execution, **matmul_add.om** model file is generated in **data** directory, which can be loaded and executed per offline inference flow.

    - Check execution result:

    - When custom Pass is effective, compare NPU compilation intermediate dump graphs, find model optimized as expected. Dump graph retrieval method: click [Link](https://hiascend.com/document/redirect/CannCommercialEnvvar) > Compilation related > Graph compilation > DUMP_GE_GRAPH:

        - For versions before 8.3.RC1, dump graph names:
          - ge_onnx_xxxxxxxx_RunCustomPassBegin.pbtxt: graph before fusion
          - ge_onnx_xxxxxxxx_RunCustomPassEnd.pbtxt: graph after fusion
        - For 8.3.RC1 and later versions, dump graph names:
          - ge_onnx_xxxxxxxx_PreRunBegin.pbtxt: graph before fusion
          - ge_onnx_xxxxxxxx_RunCustomPassBeforeInfershape.pbtxt: graph after fusion

    - Following log appears:

        ```text
        FuseMatMulAndAddPass begin.
        Find src node: MatMul.
        Find dst node: Add.
        FuseMatMulAndAddPass end.
        ```

3. Use TF online inference.

   - Online inference with and without custom pass so in target folder, execute:

     **python tf_forward.py**

     Both runs produce identical results:

     ```text
     ---out---
      [[23. 29.]
      [50. 65.]]
     ```

   - Check execution result:

     - Custom pass before and after effective produce same results.

     - When custom Pass is effective, compare NPU compilation intermediate dump graphs, find model optimized as expected:
       - For versions before 8.3.RC1, dump graph names:
         - ge_onnx_xxxxxxxx_RunCustomPassBegin.pbtxt: graph before fusion
         - ge_onnx_xxxxxxxx_RunCustomPassEnd.pbtxt: graph after fusion
       - For 8.3.RC1 and later versions, dump graph names:
         - ge_onnx_xxxxxxxx_PreRunBegin.pbtxt: graph before fusion
         - ge_onnx_xxxxxxxx_RunCustomPassBeforeInfershape.pbtxt: graph after fusion

     - Following log appears:

       ```text
       FuseMatMulAndAddPass begin.
       Find src node: MatMul.
       Find dst node: Add.
       FuseMatMulAndAddPass end.
       ```
