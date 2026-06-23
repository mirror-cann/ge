# 样例使用指导

## 功能描述

本样例以 BatchMatMulV2 展平融合 pass 为例，介绍将 BatchMatMul 展平为 MatMul 的融合 pass 实现，
提供 atc 工具离线编译模型验证方式，pass 使用 eager style api 和融合接口实现。

融合原理：将 A[b,m,k] @ B[k,n] 的 BatchMatMulV2 运算，通过 Reshape 展平为 [b*m,k]@[k,n] 的 MatMulV2 运算，
再通过 Reshape 恢复为 [b,m,n] 的输出形状。

本样例仅覆盖 A 为 3 维、B 为 2 维、两个输入 dtype 相同且为 float/float16/bfloat16、无 bias/offset_w、offset_x=0、transpose 属性均为 false 的教学场景。
带 batch 维广播、bias、offset_w 或非 0 offset_x 的 BatchMatMul/BatchMatMulV2 不在本样例优化范围内。

## 目录结构

```
├── README.md                                        // C++ 样例说明
├── src
│   ├──batch_matmul_flatten_pass.cpp                 // pass 实现文件
├── CMakeLists.txt                                   // 编译脚本
├── data
│   ├──gen_onnx.py                                   // onnx 导出脚本用于 ATC 离线验证（支持 --batch/--m/--k/--n 参数）
│   ├──quick_verify.sh                               // 一键式验证脚本（支持传入 shape 和执行次数）
│   ├──benchmark_model.cpp                           // 性能测试程序源码
├── gen_es_api
│   ├──CMakeLists.txt                                // 生成 eager style api 的编译脚本
```

## 环境要求

- 编译器：GCC >= 7.3.x
- 使用 python 及其依赖库版本：python>=3.9、onnx
- 已完成[环境准备](../../../../../docs/zh/build.md#1-环境准备)。

## 实现步骤

1. 定义类 `BatchMatmulFlattenPass` 继承 `PatternFusionPass`。
2. 重写基类 `PatternFusionPass` 中的 3 个函数：
   - `Patterns` 定义匹配模板，用于在整图中获取与该模板相同的拓扑。
     - `pattern->CaptureTensor()` 捕获 BatchMatMul 节点，用于读取属性和输入 shape。
   - `MeetRequirements` 对模板匹配到的拓扑进行筛选。
     - 检查仅存在 x1/x2 两个有效输入，输入 A 为 3 维、输入 B 为 2 维、两个输入 dtype 相同且为浮点类型、offset_x=0、transpose 属性为 false。
   - `Replacement` 定义替换部分。
     - 构建 Reshape+MatMulV2+Reshape 替换图，根据 shape 是否动态选择 Const 或动态计算 reshape 目标 shape。
     - 使用 `InferShapeAndCheckSupport` 验证替换图正确性。
3. 注册 `BatchMatmulFlattenPass` 为自定义融合 pass，执行阶段为 AfterInferShape。

## 程序编译

假设 CANN 软件包的安装目录为 INSTALL_PATH，例如 `/home/HwHiAiUser/Ascend/`。

1. 配置环境变量。

   运行软件包中设置环境变量脚本，命令如下：

   ```
   source ${ASCEND_PATH}/set_env.sh
   ```

   `${ASCEND_PATH}` 为 CANN 软件包安装目录下的 cann 路径。请替换相关软件包的实际安装路径，例如 `${INSTALL_PATH}/cann`。

2. 根据实际情况修改当前目录 `CMakeLists.txt` 文件中的如下信息。

   - ASCEND_PATH：可以设置默认的软件包路径，如果通过 `set_env.sh` 设置了 `$ASCEND_HOME_PATH`，无需修改。

   - PASS_SO_DIR：可以设置自定义融合 pass 动态库安装目录名，默认为 `pass_so_dir`。

   - target_include_directories：需要包含的头文件，对于本示例，无需修改。如果是用户自行开发的代码，当需要添加头文件时，在示例下方直接增加行即可，注意不要删除原有项目。如果网络中有自定义算子，请增加自定义算子的原型定义头文件。

   - target_link_libraries：需要链接的库，对于本示例，无需修改。如果是用户自行开发的代码，当需要添加链接库时，在示例下方直接增加行即可，注意不要删除原有项目。

     > 禁止链接软件包中的其他 so，否则后续升级可能会导致兼容性问题。

3. 依次执行：

   ```
   mkdir build && cd build
   cmake ..
   ```

   执行后，在 **build** 目录下产生的 es_all_build/generated_code 目录中包含 es 构图 api 的头文件及源码。

4. 执行 `make` 命令编译自定义 pass so，成功编译后通过 `make install` 将动态库文件 `libbatch_matmul_flatten_pass.so` 安装到自定义融合 pass 目录下。
   可以在 `make` 后增加可选参数 `-j$(nproc)` 用于并行执行构建任务，`$(nproc)` 动态获取 CPU 核心数。

   ```
   make -j$(nproc) batch_matmul_flatten_pass
   make install
   ```

   样例验证完成后，执行如下命令清理安装到 CANN 包下的自定义 pass so，避免影响后续 UT/ST：

   ```
   make clean_custom_pass
   ```

## 程序运行

1. 配置环境变量（如已执行，跳过）。

   - 运行软件包中设置环境变量脚本，命令如下：

     ```
     source ${ASCEND_PATH}/set_env.sh
     ```

     `${ASCEND_PATH}` 请替换相关软件包的实际安装路径。

2. 使用 ATC 离线推理。

   - 设置环境变量，dump 出编译过程中的模型图：

     ```
     export DUMP_GE_GRAPH=1
     ```

   - 进入当前目录 `data` 目录执行 `.py` 文件导出 onnx（文件中使用了 onnx 库，运行前请确保安装）：

     ```
     python gen_onnx.py
     ```

   - 也可指定 shape 参数导出不同 shape 的模型：

     ```
     python gen_onnx.py --batch 32 --m 64 --k 512 --n 256
     ```

   - 执行结束后，在 `data` 目录下生成 `.onnx` 格式的模型文件，名称为 `model.onnx`。
   - 执行 ATC 工具命令（关于 ATC 工具的详细说明，请前往[昇腾文档](https://www.hiascend.com/zh/document)搜索文档"ATC离线模型编译工具"），`soc_version` 请根据实际环境修改：

     ```
     atc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model_fused
     ```

   - 日志中出现如下打印：

     ```
     Define pattern for BatchMatmulFlattenPass
     Define MeetRequirements for BatchMatmulFlattenPass
     Define replacement for BatchMatmulFlattenPass
     Created node: Reshape
     Created node: MatMulV2
     Created node: Reshape
     InferShapeAndCheckSupport success
     ```

3. 一键式验证（可选）

   - 使用 `quick_verify.sh` 脚本可一键完成编译、ATC、dump 图检查和性能测试。脚本中的 `soc_version` 默认为 `Ascend910B3`，请根据实际环境修改脚本中 atc 命令的 `--soc_version` 参数（参考[使用 ATC 离线推理](#程序运行)中的说明）：

     ```
     cd data
     ./quick_verify.sh [batch] [m] [k] [n] [test_rounds]
     ```

   - 默认参数：batch=32, m=64, k=512, n=256, test_rounds=3

   - 脚本会自动：
     1. 检查并编译 Pass（如未编译）
     2. 检查并编译 benchmark_model
     3. 生成 ONNX 模型
     4. ATC 编译融合模型
     5. 检查 dump 图验证融合效果
     6. 运行多轮性能测试
      7. 清理安装到 CANN 包下的自定义 pass so

   - 若融合前 dump 图中没有 BatchMatMul 节点，或融合后没有出现 Reshape+MatMulV2+Reshape 替换结构，脚本会直接退出失败。

4. 查看运行结果

   - ATC 工具命令执行完成后，目录下生成一系列 `.pbtxt` 和 `.txt` 文件。
     对比以下 dump 图：
     - `ge_proto_xxxxx_graph_x_PreRunBegin.txt` 执行前 dump 图，应包含 BatchMatMulV2 节点
     - `ge_proto_xxxxx_graph_x_RunCustomPass_AfterInferShape.txt` 执行 InferShape 后的自定义 pass dump 图，应包含 Reshape+MatMulV2+Reshape 节点，不再包含 BatchMatMulV2 节点

     可以发现模型已按预期优化，即 BatchMatMulV2 被 Reshape+MatMulV2+Reshape 替换。

   - 若未获得预期结果，可设置如下环境变量（如使用 atc 命令，还需添加参数 `--log=debug`）让日志打印到屏幕，来定位原因：

     ```bash
     export ASCEND_SLOG_PRINT_TO_STDOUT=1
     export ASCEND_GLOBAL_LOG_LEVEL=0
     ```
