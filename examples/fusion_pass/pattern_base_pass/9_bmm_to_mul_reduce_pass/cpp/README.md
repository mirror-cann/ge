# 样例使用指导

## 功能描述

本样例以 BatchMatMulV2 转 Mul+ReduceSum 融合 pass 为例，介绍将 RecSDK 中手写图遍历的 bmm_to_mul_reduce_pass
重构为标准 PatternFusionPass 框架的实现，提供 atc 工具离线编译模型验证方式，pass 使用 eager style api 和融合接口实现。

融合原理：针对 `BatchMatMulV2` 算子在 `n=1` 且 `k<=16` 的 shape 下，将其改写为 `Mul + ReduceSumD` 利用 vector 单元计算以提升性能。

数学等价性：当 `n=1` 时，`output[b][i][0] = Σ_k A[b][i][k] * B[b][0][k]`，恰好等价于对 `Mul(A, B)` 在 k 轴做
`ReduceSum(keep_dims=true)`。其中 B 在 `n=1` 时 shape 为 `[b, 1, k]`，与 A `[b, m, k]` 做 Mul 时沿 n 维广播，
再对最后一维（k 轴）做 ReduceSumD 即可得到 `[b, m, 1]` 的输出，与原 BatchMatMulV2 输出一致。

本样例覆盖两种 pattern：

- Pattern 1：`adj_x2=true` 直接路径，BMM 的输入1直接是权重 `[b, n, k]`（n=1, k<=16）。
- Pattern 2：`adj_x2=false` 且输入1来自 Transpose（交换最后两维），Transpose 的输入为 `[b, n, k]`（n=1, k<=16）。

## 目录结构

```text
├── README.md                                        // C++ 样例说明
├── src
│   ├──bmm_to_mul_reduce_pass.cpp                    // pass 实现文件
├── CMakeLists.txt                                   // 编译脚本
├── data
│   ├──gen_onnx.py                                   // onnx 导出脚本用于 ATC 离线验证（支持 --batch/--m/--k 参数）
│   ├──quick_verify.sh                               // 一键式验证脚本（支持传入 shape）
├── gen_es_api
│   ├──CMakeLists.txt                                // 生成 eager style api 的编译脚本
```

## 环境要求

- 编译器：GCC >= 7.3.x
- 使用 python 及其依赖库版本：python>=3.9、onnx
- 已完成[环境准备](../../../../../docs/zh/build.md#1-环境准备)。

## 实现步骤

1. 定义类 `BmmToMulReducePass` 继承 `PatternFusionPass`。
2. 重写基类 `PatternFusionPass` 中的 3 个函数：
   - `Patterns` 定义两个匹配模板：
     - Pattern 1：`Input0 → BMM(adj_x2=true) ← Input1`，捕获 BMM 节点。
     - Pattern 2：`Input0 → BMM(adj_x2=false) ← Transpose ← Input1`，捕获 BMM 和 Transpose 节点。
   - `MeetRequirements` 对模板匹配到的拓扑进行筛选：
     - 检查 `adj_x1 == false`、dtype 为 float/float16、BMM 输出不超过 1 个消费者。
     - Pattern 1：`adj_x2 == true`，n=input1_shape[dims-2]，k=input1_shape[dims-1]。
     - Pattern 2：`adj_x2 == false`，Transpose 确实交换最后两维，n=Transpose_input_shape[dims-2]，k=Transpose_input_shape[dims-1]。
     - 满足 `n == 1 && k <= 16`。
   - `Replacement` 定义替换部分：
     - 构建 `Mul(Input0, Input1) → ReduceSumD(axis=-1, keep_dims=true)` 替换图。
     - 使用 `InferShapeUtil::InferShape` 推导 shape。
3. 注册 `BmmToMulReducePass` 为自定义融合 pass，执行阶段为 AfterInferShape。

## 程序编译

假设 CANN 软件包的安装目录为 INSTALL_PATH，例如 `/home/HwHiAiUser/Ascend/`。

1. 配置环境变量。

   运行软件包中设置环境变量脚本，命令如下：

```bash
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

```bash
   mkdir build && cd build
   cmake ..
```

   执行后，在 **build** 目录下产生的 es_all_build/generated_code 目录中包含 es 构图 api 的头文件及源码。

4. 执行 `make` 命令编译自定义 pass so，成功编译后通过 `make install` 将动态库文件 `libbmm_to_mul_reduce_pass.so` 安装到自定义融合 pass 目录下。
   可以在 `make` 后增加可选参数 `-j$(nproc)` 用于并行执行构建任务，`$(nproc)` 动态获取 CPU 核心数。

```bash
   make -j$(nproc) bmm_to_mul_reduce_pass
   make install
```

5. 编译过程中会在 build 目录下生成 pass so 依赖的 es so（位于 `build/es_output/lib64`，文件名为 `libes_all.so`）。`make install` 仅安装 pass so，es so 仍留在 build 目录。CMakeLists.txt 中已通过 `$ORIGIN` 与构建目录路径配置运行时查找路径：
   - 若 build 目录保留在原位，pass so 运行时可直接通过构建路径找到 es so，无需额外操作。
   - 若 build 目录被删除或 pass so 迁移到其他位置（运行时无法再访问原构建路径），需将 es so 拷贝到 pass so 的安装目录（即 `${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes`）与 pass so 同目录存放，运行时通过 `$ORIGIN` 从同目录加载，无需额外设置 `LD_LIBRARY_PATH`。

```bash
   cp build/es_output/lib64/libes_all.so ${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes/
```

   样例验证完成后，执行如下命令清理安装到 CANN 包下的自定义 pass so，避免影响后续 UT/ST：

```bash
   make clean_custom_pass
```

## 程序运行

1. 配置环境变量（如已执行，跳过）。

   - 运行软件包中设置环境变量脚本，命令如下：

```bash
     source ${ASCEND_PATH}/set_env.sh
```

   - `${ASCEND_PATH}` 请替换相关软件包的实际安装路径。

2. 使用 ATC 离线推理。

   - 设置环境变量，dump 出编译过程中的模型图：

```bash
     export DUMP_GE_GRAPH=1
```

   - 进入当前目录 `data` 目录执行 `.py` 文件导出 onnx（文件中使用了 onnx 库，运行前请确保安装）：

```bash
     python gen_onnx.py
```

   - 也可指定 shape 参数导出不同 shape 的模型：

```bash
     python gen_onnx.py --batch 32 --m 64 --k 8
```

   - 执行结束后，在 `data` 目录下生成 `.onnx` 格式的模型文件，名称为 `model.onnx`。
   - 执行 ATC 工具命令（关于 ATC 工具的详细说明，请前往[昇腾文档](https://www.hiascend.com/zh/document)搜索文档"ATC离线模型编译工具"），`soc_version` 请根据实际环境修改：

```bash
     atc --model=./model.onnx --framework=5 --soc_version=Ascend910B3 --output=./model_fused
```

   - 日志中出现如下打印：

```text
     Define pattern for BmmToMulReducePass
     Define MeetRequirements for BmmToMulReducePass
     Define replacement for BmmToMulReducePass
     Created node: Mul
     Created node: ReduceSumD
     Replacement success
```

3. 一键式验证（可选）

   - 使用 `quick_verify.sh` 脚本可一键完成编译、ATC、dump 图检查。脚本中的 `soc_version` 默认为 `Ascend910B3`，请根据实际环境修改脚本中 atc 命令的 `--soc_version` 参数（参考[使用 ATC 离线推理](#程序运行)中的说明）：

```bash
     cd data
     ./quick_verify.sh [batch] [m] [k]
```

   - 默认参数：batch=100, m=2333, k=4

   - 脚本会自动：
     1. 检查并编译 Pass（如未编译）
     2. 生成 ONNX 模型
     3. ATC 编译融合模型
     4. 检查 dump 图验证融合效果
     5. 清理安装到 CANN 包下的自定义 pass so

   - 若融合前 dump 图中没有 BatchMatMulV2 节点，或融合后没有出现 Mul+ReduceSumD 替换结构，脚本会直接退出失败。

4. 查看运行结果

   - ATC 工具命令执行完成后，目录下生成一系列 `.pbtxt` 和 `.txt` 文件。
     对比以下 dump 图：
     - `ge_proto_xxxxx_graph_x_PreRunBegin.txt` 执行前 dump 图，应包含 BatchMatMulV2 节点
     - `ge_proto_xxxxx_graph_x_RunCustomPass_AfterInferShape.txt` 执行 InferShape 后的自定义 pass dump 图，应包含 Mul+ReduceSumD 节点，不再包含 BatchMatMulV2 节点

     可以发现模型已按预期优化，即 BatchMatMulV2 被 Mul+ReduceSumD 替换。

   - 若未获得预期结果，可设置如下环境变量（如使用 atc 命令，还需添加参数 `--log=debug`）让日志打印到屏幕，来定位原因：

     ```bash
     export ASCEND_SLOG_PRINT_TO_STDOUT=1
     export ASCEND_GLOBAL_LOG_LEVEL=0
     ```
