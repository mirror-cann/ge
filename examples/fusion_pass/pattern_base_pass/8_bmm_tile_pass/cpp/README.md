# 样例使用指导

## 功能描述

本样例以 BmmTile 融合 pass 为例，介绍如何使用 PatternFusionPass 框架消除计算图中"共享权重 + 批量输入"场景下冗余的 Tile 算子，
利用 BatchMatMulV2 自身的 batch 广播能力替代显式数据扩展，提供 atc 工具离线编译模型验证方式。

融合原理：当权重 shape 的首维为 1（如 `[1, k, n]`），被 Tile 扩展到 `[batch, k, n]` 后与输入 `[batch, m, k]` 做 BatchMatMulV2 时，
由于 BatchMatMulV2 支持首维广播，可直接删除 Tile，将 `[1, k, n]` 直连 BatchMatMulV2，由 BMM 自动广播完成计算。

本样例覆盖两类等价模式：

- Pattern 1：`Tile → BatchMatMulV2`，删除 Tile，Tile 输入直连 BMM。
- Pattern 2：`Tile → Reshape → Transpose → BatchMatMulV2`，删除 Tile，Tile 输入直连 Reshape；Reshape 目标 shape 首维改为 0（从输入拷贝），Reshape/Transpose/BMM 的首维变为 1。

每类模式中 Tile 可能在 BMM 的 input0 或 input1 侧，共 4 个 pattern。

## 目录结构

```text
├── README.md                                        // C++ 样例说明
├── src
│   ├──bmm_tile_pass.cpp                             // pass 实现文件
├── CMakeLists.txt                                   // 编译脚本
├── data
│   ├──gen_onnx.py                                   // onnx 导出脚本用于 ATC 离线验证（支持 --batch/--m/--k/--n 参数）
│   ├──quick_verify.sh                               // 一键式验证脚本（支持传入 shape 和执行次数）
├── gen_es_api
│   ├──CMakeLists.txt                                // 生成 eager style api 的编译脚本
```

## 环境要求

- 编译器：GCC >= 7.3.x
- 使用 python 及其依赖库版本：python>=3.9、onnx、numpy
- 已完成[环境准备](../../../../../docs/zh/build.md#1-环境准备)。

## 实现步骤

1. 定义类 `BmmTilePass` 继承 `PatternFusionPass`。
2. 重写基类 `PatternFusionPass` 中的 3 个函数：
   - `Patterns` 定义 4 个匹配模板，覆盖 Tile 在 BMM input0/input1 侧、以及是否经过 Reshape→Transpose 的组合。
     - Pattern 1a/1b：`Tile → BMM`，Tile 分别在 input0 和 input1 侧。
     - Pattern 2a/2b：`Tile → Reshape → Transpose → BMM`，Tile 分别在 input0 和 input1 侧。
     - 每个 pattern 通过 `CaptureTensor` 捕获 Tile 和 BMM 节点；Pattern 2 额外捕获 Reshape 和 Transpose 节点。
   - `MeetRequirements` 对模板匹配到的拓扑进行筛选。
     - 通用检查：Tile 输出仅 1 个消费者；Tile 输入第 0 维 == 1 且 BMM 另一侧第 0 维 != 1；Tile multiples 仅首维非 1。
     - Pattern 2 额外检查：Transpose perm 为 Const 且 `perm[0] == 0`；Reshape shape 为 Const；Tile multiples 为 Const 且 `multiples[0] == Reshape 首维`，其余维度 == 1；Reshape/Transpose 各仅 1 消费者。
   - `Replacement` 定义替换部分。
     - Pattern 1：直接 `BatchMatMulV2(tile_input, bmm_other)`，无 Tile。
     - Pattern 2：`Reshape(tile_input, new_shape) → Transpose → BMM`，new_shape 首维改为 0（从输入拷贝，等价于首维为 1）。
     - 使用 `InferShapeUtil::InferShape` 推导替换图 shape。
3. 注册 `BmmTilePass` 为自定义融合 pass，执行阶段为 AfterInferShape。

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

4. 执行 `make` 命令编译自定义 pass so，成功编译后通过 `make install` 将动态库文件 `libbmm_tile_pass.so` 安装到自定义融合 pass 目录下。
   可以在 `make` 后增加可选参数 `-j$(nproc)` 用于并行执行构建任务，`$(nproc)` 动态获取 CPU 核心数。

```bash
   make -j$(nproc) bmm_tile_pass
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

     `${ASCEND_PATH}` 请替换相关软件包的实际安装路径。

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
     python gen_onnx.py --batch 4 --m 128 --k 256 --n 512
```

   - 执行结束后，在 `data` 目录下生成 `.onnx` 格式的模型文件，名称为 `model.onnx`。
   - 执行 ATC 工具命令（关于 ATC 工具的详细说明，请前往[昇腾文档](https://www.hiascend.com/zh/document)搜索文档"ATC离线模型编译工具"），`soc_version` 请根据实际环境修改：

```bash
     atc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model_fused
```

   - 日志中出现如下打印：

```text
     Define pattern for BmmTilePass
     Define MeetRequirements for BmmTilePass
     Define replacement for BmmTilePass
     Created node: BatchMatMulV2
     InferShape success
```

3. 一键式验证（可选）

   - 使用 `quick_verify.sh` 脚本可一键完成编译、ATC、dump 图检查和性能测试。脚本中的 `soc_version` 默认为 `Ascend910B3`，请根据实际环境修改脚本中 atc 命令的 `--soc_version` 参数（参考[使用 ATC 离线推理](#程序运行)中的说明）：

```bash
     cd data
     ./quick_verify.sh [batch] [m] [k] [n] [test_rounds]
```

   - 默认参数：batch=2, m=64, k=128, n=256, test_rounds=3

   - 脚本会自动：
     1. 检查并编译 Pass（如未编译）
     2. 生成 ONNX 模型
     3. ATC 编译融合模型
     4. 检查 dump 图验证融合效果
     5. 运行多轮性能测试（如有 benchmark_model）
     6. 清理安装到 CANN 包下的自定义 pass so

   - 若融合前 dump 图中没有 Tile 节点，或融合后 Tile 节点未被删除，脚本会直接退出失败。

4. 查看运行结果

   - ATC 工具命令执行完成后，目录下生成一系列 `.pbtxt` 和 `.txt` 文件。
     对比以下 dump 图：
     - `ge_proto_xxxxx_graph_x_PreRunBegin.txt` 执行前 dump 图，应包含 Tile 和 BatchMatMulV2 节点
     - `ge_proto_xxxxx_graph_x_RunCustomPass_AfterInferShape.txt` 执行 InferShape 后的自定义 pass dump 图，应不再包含 Tile 节点，BatchMatMulV2 保留

     可以发现模型已按预期优化，即冗余 Tile 被删除，BatchMatMulV2 利用首维广播完成计算。

   - 若未获得预期结果，可设置如下环境变量（如使用 atc 命令，还需添加参数 `--log=debug`）让日志打印到屏幕，来定位原因：

     ```bash
     export ASCEND_SLOG_PRINT_TO_STDOUT=1
     export ASCEND_GLOBAL_LOG_LEVEL=0
     ```
