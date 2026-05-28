# Data Dependent Shape 自定义算子(三类算子)样例

## 样例概述

- 构图入口：`GE`
- 算子编成语言：`Ascend C`
- 编译方式：`.asc` 与 `.cpp` 同 target 编译
- 模型下沉能力：`不涉及`
- 核心链路：`Ascend C kernel 与 host 侧 custom op 同库编译 -> GE 交付件 -> 进程内构图 -> Session::RunGraph -> hybrid/RT2 动态执行`
- 与其他 sample 的区别：本样例聚焦三类自定义算子在 `unknown graph` 动态执行场景下的 shape buffer 用法。

本样例展示一条最小可运行的三类自定义算子执行链路：在进程内构图后直接调用 `Session::RunGraph`，由 CMake 将 Ascend C kernel 和 host 侧 custom op 编译到同一个 `libwhere_like_custom_op.so`，再由 `WhereLikeCustom::Execute()` 申请最大输出和 shape buffer，最终在 device 侧写回实际输出 shape。

`WhereLikeCustom` 输入为 `bool[N]`，用于返回输入中值为 `true` 的元素下标，输出为 `int64[true_count, rank]`。由于 `true_count` 依赖运行时输入数据，编译期只能确定输出上界，实际 shape 需要通过三类 custom op 的 shape buffer 协议在执行后回传。

## 适用场景

- 想看三类自定义算子在 `Session::RunGraph` 动态执行链路中的最小实现方式。
- 想参考用户在 `Execute` 内自行同步 stream、回读 shape buffer 并更新输出 shape 的方式。
- 想看 `.asc` 和 `.cpp` 直接一起编译，并在 `Execute` 中发起 kernel 调用的完整流程。
- 不适合用于了解 `ATC离线编译 -> om离线模型` 下沉链路。

## 前置依赖

### CANN

- 已正确安装并配置 CANN 环境，例如执行过 `source ${ASCEND_HOME_PATH}/set_env.sh`。
- 当前环境具备 `ACL`、`GE`、`Graph`、`Ascend C` 相关头文件与库。
- 参考 [安装指导](../../../docs/build.md#2-安装软件包) 完成 toolkit 和 ops 包安装。

### 框架与插件

- 本样例不依赖 PyTorch、TensorFlow 或 TorchAir。

### 环境变量

- `ASCEND_HOME_PATH`
- `ASCEND_CUSTOM_OPP_PATH` 会在 `run.sh` 中自动追加为当前 sample 的 `output/`

### 额外依赖

- `cmake`
- `g++`

## 快速运行

在 `examples/custom_op/data_dependent_shape_custom` 目录下执行：

### 推荐方式

```bash
source ${ASCEND_HOME_PATH}/set_env.sh
bash run.sh
```

`run.sh` 会自动完成 configure、build、install，并把 `output/` 追加到 `ASCEND_CUSTOM_OPP_PATH`。若运行成功，终端会打印：

```text
output shape: [4, 1]
output values: 0 2 4 7
```

### 分步方式

```bash
source ${ASCEND_HOME_PATH}/set_env.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"

cd build
./data_dependent_shape_custom_session_run
cd ..
```

其中 `export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"` 用于将自定义算子包根目录加入环境变量，随后 GE 会按 `output/op_graph/lib/<os>/<arch>/libwhere_like_custom_op.so` 规则加载交付件。

## 目录结构与关键文件

```text
data_dependent_shape_custom
├── CMakeLists.txt
├── README.md
├── run.sh
├── ge
│   ├── custom_op.cpp                    // EagerExecuteOp + ShapeInferOp 主流程实现
│   ├── where_like_custom.h       // WhereLikeCustom proto 定义
│   └── where_like_custom_kernel.asc // Ascend C kernel 源码
└── session_run
    └── main.cc                          // 进程内构图并直接调用 Session::RunGraph
```

重点文件：

- `ge/custom_op.cpp`
  自定义算子的核心主流程，实现 `Execute`、`InferShape` 和 `InferDataType`；其中 `Execute` 负责申请最大输出、shape buffer、调用 `.asc` 中导出的 launch wrapper，并在 kernel 完成后自行回读 shape buffer 更新输出 shape，`InferShape` / `InferDataType` 负责编译期输出 shape / dtype 推导。
- `ge/where_like_custom_kernel.asc`
  device kernel 和 host 侧 launch wrapper 实现，负责写输出数据和 shape buffer。
- `session_run/main.cc`
  构建最小图并直接通过 `Session::AddGraph + Session::RunGraph` 执行。
- `run.sh`
  串起 configure、build、install 和运行过程。

## 核心链路

1. `session_run/main.cc` 构建包含 `Data -> WhereLikeCustom` 的最小图，并把输入/输出描述设置为动态 shape，使整图走 `unknown graph` 执行链路。
2. `ge/custom_op.cpp` 中的 `InferShape` / `InferDataType` 在构图阶段给出输出 shape / dtype；本样例不依赖框架 lowering 插入 shape 回写节点。
3. `CMakeLists.txt` 将 `ge/custom_op.cpp` 和 `ge/where_like_custom_kernel.asc` 一起编译到 `libwhere_like_custom_op.so`。
4. `ge/custom_op.cpp` 在 `Execute` 回调中按最大 shape 调用 `ctx->MallocOutputTensor(...)` 申请输出，再通过第一次 `ctx->MallocWorkSpace(...)` 申请 shape buffer。
5. `ge/where_like_custom_kernel.asc` 在 device 侧写入输出索引和 shape buffer，其中 shape buffer 用于回传实际输出 shape。
6. `ge/custom_op.cpp` 的 `Execute` 在 launch 后同步当前 stream，将 shape buffer 拷回 host，解析真实 shape 并更新输出 tensor 的 logical shape 和有效 size。
7. 执行完成后，`session_run/main.cc` 读取输出 tensor，打印实际 shape 和输出值。

## 构建产物

- `output/op_graph/lib/linux/x86_64/libwhere_like_custom_op.so`
  Linux x86_64 环境下 GE 使用的自定义算子交付件；aarch64 环境对应 `output/op_graph/lib/linux/aarch64/libwhere_like_custom_op.so`。
- `output/op_graph/include/where_like_custom.h`
  构图侧可直接使用的算子 proto 头文件。
- `build/data_dependent_shape_custom_session_run`
  直接 `Session::RunGraph` 的执行程序。

## 结果校验

成功时可观察到：

- `output/op_graph/lib/<os>/<arch>/libwhere_like_custom_op.so` 已生成。
- `output/op_graph/include/where_like_custom.h` 已生成。
- 终端输出包含 `output shape: [4, 1]`。
- 终端输出包含 `output values: 0 2 4 7`。

若失败，优先检查：

- `ASCEND_HOME_PATH` 是否已设置并已正确 `source` CANN 环境。
- `ASCEND_CUSTOM_OPP_PATH` 是否已包含当前 sample 的 `output/`。
- `output/op_graph/lib/<os>/<arch>/libwhere_like_custom_op.so` 和 `output/op_graph/include/where_like_custom.h` 是否已生成。
- 当前环境是否具备可用 NPU 和可用 Ascend C 编译环境。

## 注意事项 / 限制
- `WhereLikeCustom` 当前样例输入为一维 `bool[8]`，因此实际输出为匹配位置索引，shape 为 `[true_count, 1]`。
- `.asc` 编译参数当前固定为 `--npu-arch=dav-2201`，如目标芯片不同需调整 `CMakeLists.txt`。

## 附录

### 算子规格

| 项目 | 内容 |
| --- | --- |
| 算子类型 | `WhereLikeCustom` |
| 输入 | `x` |
| 输出 | `y` |
| 输入 shape | `N` |
| 输出 shape 上界 | `[N, rank(x)]` |
| 输出实际 shape | `[true_count, rank(x)]` |
| 输入数据类型 | `bool` |
| 输出数据类型 | `int64` |
| 格式 | `ND` |
| kernel 名称 | `where_like_custom` |

### shape buffer 约定

本样例由用户自定义 shape buffer 协议，并在 `Execute` 中自行解析回写实际输出 shape。当前 kernel 会写入：

- `shape[0] = 2U`
- `shape[1] = true_count`
- `shape[2] = rank`

对当前一维输入场景，`rank = 1`，因此输入 `[true, false, true, false, true, false, false, true]` 的实际输出 shape 为 `[4, 1]`，输出数据为 `0 2 4 7`。
