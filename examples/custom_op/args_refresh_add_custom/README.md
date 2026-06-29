# ArgsUpdater 地址刷新自定义算子样例

## 样例概述

- 构图入口：`GE`
- 算子编程语言：`Ascend C`（RTC 运行时编译）
- 编译方式：`.cpp` 编译 host 侧 custom op，kernel 源码通过 RTC 在运行时编译为 device binary
- 核心链路：`Ascend C kernel 源码 -> RTC 运行时编译 -> GE 交付件 -> 进程内构图 -> Session::ExecuteGraphWithStreamAsync 在线执行`
- 与其他 sample 的区别：本样例聚焦 `ArgsUpdater` 接口与 `MallocReadOnlyDevArgs` 配合实现地址刷新，通过 GE 框架管理 args 同步，避免额外的 D2D 拷贝（MEMCPY_ASYNC），从而提升重复执行性能。

本样例展示 `ArgsUpdater` 地址刷新机制的完整链路：以 Ascend C Add 算子为例，输入 shape 为 `[4096, 4096]` float32（16M 元素，64MB），定义两个功能相同的算子——`AddRefreshOp`（实现 `ArgsUpdater` 接口）和 `AddNoRefreshOp`（不实现），通过 `Session::ExecuteGraphWithStreamAsync` 在线执行对比两者的性能差异。

`ArgsUpdater` 的核心思想：模型加载时通过 `MallocReadOnlyDevArgs` 在 device 侧分配一块只读的 kernel args 内存，后续重复执行时由 `UpdateHostArgs` 回调仅刷新 args 中的地址字段（输入/输出 tensor 指针）。这消除了 GE 框架为同步算子的输入输出 tensor 内容到 device 侧而插入的额外 D2D 拷贝（MEMCPY_ASYNC），在高频执行场景下可带来约 1.17x 的性能提升。

## 适用场景

- 想了解 `ArgsUpdater` + `MallocReadOnlyDevArgs` 地址刷新机制的实现方式和性能收益。
- 想看 Ascend C kernel 通过 RTC 运行时编译后，在 GE 自定义算子中调用的完整流程。
- 想对比有/无地址刷新两种实现在高频执行下的性能差异。

## 前置依赖

### CANN

- 已正确安装并配置 CANN 环境，例如执行过 `source ${ASCEND_HOME_PATH}/set_env.sh`。
- 当前环境具备 `ACL`、`GE`、`Graph` 相关头文件与库。
- 参考 [安装指导](../../../docs/zh/quick_install.md) 完成 toolkit 和 ops 包安装。

### 框架与插件

- 本样例不依赖 PyTorch、TensorFlow 或 TorchAir。
- kernel 源码 `add_custom_kernel/add_custom.asc` 通过 RTC 在运行时编译，无需预编译。

### 环境变量

- `ASCEND_HOME_PATH`
- `ASCEND_CUSTOM_OPP_PATH` 会在 `run.sh` 中自动追加为当前 sample 的 `output/`

### 额外依赖

- `cmake`
- `g++`

## 快速运行

在 `examples/custom_op/args_refresh_add_custom` 目录下执行：

### 推荐方式

```bash
source ${ASCEND_HOME_PATH}/set_env.sh
bash run.sh
```

`run.sh` 会自动完成 configure、build、install，并把 `output/` 追加到 `ASCEND_CUSTOM_OPP_PATH`。脚本依次执行以下 2 个步骤：

1. 编译自定义算子交付件和执行程序
2. 运行 `session_run`（在线性能对比）

若运行成功，终端会打印类似：

```text
[Perf] input shape: [4096, 4096], float32, 64MB
[Perf] iters: 100
[Perf] With    ArgsUpdater: xxx us (avg xxx us/iter)
[Perf] Without ArgsUpdater: xxx us (avg xxx us/iter)
[Perf] Speedup: xxx x
```

### 分步方式

```bash
source ${ASCEND_HOME_PATH}/set_env.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"

# 在线执行：性能对比
cd build
./args_refresh_session_run
cd ..
```

其中 `export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"` 用于将自定义算子包根目录加入环境变量，随后 GE 会按 `output/op_graph/lib/<os>/<arch>/libcust_opapi.so` 规则加载交付件。

## 目录结构与关键文件

```text
args_refresh_add_custom
├── CMakeLists.txt
├── README.md
├── run.sh
├── add_custom_kernel
│   ├── add_custom.asc                // Ascend C Add kernel 源码（RTC 运行时编译）
│   └── add_custom_kernel.h           // kernel 头文件
├── ge
│   ├── add_custom.h                  // AddRefreshOp / AddNoRefreshOp proto 定义
│   ├── custom_op.cpp                 // 两个算子的 Execute、ArgsUpdater、InferShape 等实现
│   └── utils
│       ├── log.h                     // 统一日志宏（LOG_ERROR/LOG_WARNING/LOG_INFO）
│       ├── rtc_kernel_loader.h       // RTC kernel 加载器接口
│       └── rtc_kernel_loader.cpp     // RTC 编译和加载实现
└── session_run
    └── main.cc                       // 进程内构图，在线性能对比
```

重点文件：

- `ge/custom_op.cpp`
  自定义算子的核心主流程。`AddRefreshOp` 同时实现 `EagerExecuteOp`、`ArgsUpdater` 和 `ShapeInferOp`；`AddNoRefreshOp` 仅实现 `EagerExecuteOp` 和 `ShapeInferOp`。两者都通过 `RtcKernelLoader` 加载 kernel、分配输出 tensor、调用 `aclrtLaunchKernelV2` 发起 kernel。区别在于 `AddRefreshOp` 通过 `MallocReadOnlyDevArgs` 注册 args 并实现 `UpdateHostArgs` 回调，由 GE 框架管理 args 同步；`AddNoRefreshOp` 未注册 args，GE 框架无法感知地址变化，每次执行都需要插入额外的 D2D 拷贝（MEMCPY_ASYNC）来同步算子的输入输出 tensor 内容到 device 侧。
- `ge/utils/rtc_kernel_loader.cpp`
  RTC kernel 加载器，封装了从源码编译到加载的完整流程：读取 kernel 源码 → `aclrtcCreateProg` → `aclrtcCompileProg` → `aclrtcGetBinData` → `aclrtBinaryLoadFromData` → `aclrtBinaryGetFunction`。支持动态获取 NPU 架构生成编译选项。
- `ge/utils/log.h`
  统一日志宏，支持 `LOG_ERROR`、`LOG_WARNING`、`LOG_INFO` 三个级别，自动附加文件名和行号。
- `ge/add_custom.h`
  构图侧算子 proto 定义，注册 `AddRefreshOp` 和 `AddNoRefreshOp`。
- `add_custom_kernel/add_custom.asc`
  Ascend C Add kernel 源码，按 `BLOCK_SIZE=1024` 做 element-wise 加法，通过 RTC 在运行时编译。
- `session_run/main.cc`
  构建两张图（分别使用 `AddRefreshOp` 和 `AddNoRefreshOp`），通过 `Session::ExecuteGraphWithStreamAsync` 执行并进行 100 轮性能对比。使用两组内存交替触发 `UpdateHostArgs` 地址变化。
- `run.sh`
  串起编译和在线执行的完整流程。

## 核心链路

### 在线执行（Session::ExecuteGraphWithStreamAsync）

1. `session_run/main.cc` 构建两张图：`refresh_graph`（使用 `AddRefreshOp`）和 `no_refresh_graph`（使用 `AddNoRefreshOp`），输入 shape 均为 `[4096, 4096]` float32。
2. `ge/custom_op.cpp` 中 `Execute` 回调在模型加载时通过 `RtcKernelLoader` 编译并加载 kernel，通过 `ctx->MallocOutputTensor(...)` 分配输出 tensor。注意：`Execute` 只在模型加载时调用一次，后续模型下沉到设备执行时不再调用。
3. `AddRefreshOp` 通过 `ctx->MallocReadOnlyDevArgs(...)` 将 `AddArgs` 结构体注册到 GE 框架，并额外实现 `UpdateHostArgs` 回调：在后续每次执行时，GE 框架调用此回调刷新 host 侧 args 中的输入/输出 tensor 地址，然后由 GE 框架负责将变更高效同步到 device 侧。
4. `AddNoRefreshOp` 未实现 `ArgsUpdater`，也未使用 `MallocReadOnlyDevArgs` 注册 args。虽然 Execute 中的 `aclrtMalloc` + `aclrtMemcpy` 只在模型加载时发生一次，但由于未注册 args，GE 框架无法感知地址变化，需要在图中插入额外的 Identity 算子来搬运数据，每次执行产生额外的 D2D 拷贝（MEMCPY_ASYNC）来同步算子的输入输出 tensor 内容到 device 侧。
5. 两者均通过 `aclrtLaunchKernelV2` 下发 Ascend C kernel。
6. 执行完成后，`session_run/main.cc` 统计并打印两者的总耗时和加速比。

### ArgsUpdater + MallocReadOnlyDevArgs 机制

```
模型加载时（Execute 仅调用一次）:
  Execute()
    ├─ RtcKernelLoader::Load()                        → RTC 编译并加载 kernel
    ├─ MallocReadOnlyDevArgs(&args, sizeof(args))     → 分配 device 侧只读 args 内存
    ├─ 填充 AddArgs { x_ptr, y_ptr, z_ptr }
    └─ aclrtLaunchKernelV2(registered_args)           → kernel 下发

后续每次执行 (AddRefreshOp):
  UpdateHostArgs(ctx)
    ├─ GetKernelArgs(kPlacementHost, 0)               → 获取 host 侧 args 指针
    └─ 仅刷新 args->x_ptr / y_ptr / z_ptr             → 更新 tensor 地址
  GE 框架自动将地址变更同步到 device 侧，无需重新拷贝 args
```

`MallocReadOnlyDevArgs` 在模型加载时将 args 结构体拷贝到 device 侧并缓存；后续每次执行时 `UpdateHostArgs` 仅更新 host 侧 args 中的地址字段，GE 框架负责将变更同步到 device 侧，避免了 GE 框架为同步算子的输入输出 tensor 内容到 device 侧而插入的额外 D2D 拷贝（MEMCPY_ASYNC）。

### RTC 运行时编译

```
RtcKernelLoader::Load()
  ├─ GetCurrentLibraryDir()                           → 获取动态库目录
  ├─ LoadTextFromFile(source_path)                    → 读取 kernel 源码
  ├─ GetRtcCompileOption()                            → 动态获取 NPU 架构（如 dav-2201）
  ├─ aclrtcCreateProg()                               → 创建编译程序
  ├─ aclrtcCompileProg()                              → 编译 kernel
  ├─ aclrtcGetBinData()                               → 获取编译后的 binary
  ├─ aclrtBinaryLoadFromData()                        → 加载 binary
  └─ aclrtBinaryGetFunction()                         → 获取函数句柄
```

RTC 编译在模型加载时完成，后续执行直接复用已编译的 kernel，无需重复编译。

## 构建产物

- `output/op_graph/lib/linux/x86_64/libcust_opapi.so`
  Linux x86_64 环境下 GE 使用的自定义算子交付件；aarch64 环境对应 `output/op_graph/lib/linux/aarch64/libcust_opapi.so`。
- `output/op_graph/lib/<os>/<arch>/add_custom.asc`
  kernel 源码文件，由 CMake 从 `add_custom_kernel/` 拷贝而来，供 RTC 编译使用。
- `output/op_graph/include/add_custom.h`
  构图侧可直接使用的算子 proto 头文件。
- `build/args_refresh_session_run`
  在线执行的性能对比程序（`Session::ExecuteGraphWithStreamAsync`）。

## 结果校验

成功时可观察到：

- `output/op_graph/lib/<os>/<arch>/libcust_opapi.so` 已生成。
- `output/op_graph/include/add_custom.h` 已生成。
- `session_run` 终端输出包含 `[Perf] Speedup: xxx x`，且 `With ArgsUpdater` 耗时低于 `Without ArgsUpdater`。

若失败，优先检查：

- `ASCEND_HOME_PATH` 是否已设置并已正确 `source` CANN 环境。
- `ASCEND_CUSTOM_OPP_PATH` 是否已包含当前 sample 的 `output/`。
- `output/op_graph/lib/<os>/<arch>/libcust_opapi.so` 和 `output/op_graph/include/add_custom.h` 是否已生成。
- 当前环境是否具备可用 NPU。

## 注意事项 / 限制

- kernel 通过 RTC 在运行时编译，模型加载时会有编译开销，后续执行直接复用。
- RTC 编译选项通过 `aclrtGetDeviceInfo` 动态获取 NPU 架构，自动适配不同芯片型号。
- 性能对比结果受 NPU 型号、系统负载等因素影响，加速比仅供参考。
- `session_run` 中 `ge.graphRunMode` 设置为 `1`（即 `PRIORITY_GRAPH` 模式），确保走在线执行链路。
- `AddRefreshOp` 和 `AddNoRefreshOp` 的 kernel 逻辑完全相同，性能差异来自 GE 框架为同步算子的输入输出 tensor 内容到 device 侧而插入的 D2D 拷贝（MEMCPY_ASYNC）。
- 性能测试使用两组内存交替执行，触发 `UpdateHostArgs` 地址变化，更真实地体现优化效果。

## 附录

### 算子规格

| 项目 | 内容 |
| --- | --- |
| 算子类型 | `AddRefreshOp` / `AddNoRefreshOp` |
| 输入 | `x`, `y` |
| 输出 | `z` |
| 输入 shape | `[4096, 4096]` |
| 输出 shape | `[4096, 4096]` |
| 输入数据类型 | `float32` |
| 输出数据类型 | `float32` |
| 格式 | `ND` |
| kernel 名称 | `add_custom`（Ascend C，RTC 运行时编译） |
| BLOCK_SIZE | `1024` |

### ArgsUpdater 接口说明

| 接口 | 所属类 | 用途 |
| --- | --- | --- |
| `EagerExecuteOp::Execute` | `AddRefreshOp` / `AddNoRefreshOp` | 模型加载时：加载 kernel、分配输出、分配 device args、发起 kernel |
| `ArgsUpdater::UpdateHostArgs` | `AddRefreshOp` | 后续执行：获取 host 侧 args，刷新 tensor 地址字段 |
| `ShapeInferOp::InferShape` | `AddRefreshOp` / `AddNoRefreshOp` | 编译期输出 shape 推导（与输入相同） |
| `ShapeInferOp::InferDataType` | `AddRefreshOp` / `AddNoRefreshOp` | 编译期输出 dtype 推导（与输入相同） |

### 性能分析

通过 profiling 数据分析，`AddNoRefreshOp` 每轮多出约 323 us 的开销，主要来源（以下数据仅供参考，实际耗时因 NPU 型号、系统负载等因素可能有差异）：

| 开销来源 | 耗时 | 占比 |
| --- | --- | --- |
| MEMCPY_ASYNC（D2D 拷贝） | ~308 us | 95% |
| Identity 算子调度开销 | ~15 us | 5% |

`AddNoRefreshOp` 由于未通过 `MallocReadOnlyDevArgs` 注册 args，GE 框架在图编译阶段会插入额外的 Identity 算子来搬运数据。这些 Identity 算子在设备侧执行时产生 MEMCPY_ASYNC（D2D 拷贝），每次约 102 us，每轮 3 次。而 `AddRefreshOp` 通过 `UpdateHostArgs` 回调刷新地址，GE 框架能高效同步，无需插入 Identity 算子。
