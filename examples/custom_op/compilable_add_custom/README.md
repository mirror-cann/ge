# Ascend C 自定义算子算子运行时编译并下沉 om离线模型样例

## 样例概述

- 构图入口：`GE构图 + ATC离线编译`
- 算子编成语言：`Ascend C`
- 编译方式：`RTC算子运行时编译`
- 模型下沉能力：`支持下沉om离线模型`
- 核心链路：`Ascend C kernel 源码 RTC算子运行时编译 -> GE 交付件 -> AIR -> ATC离线编译 -> om离线模型 -> ACL 执行`
- 与其他 sample 的区别：本样例在 `Compile` 回调中完成 Ascend C kernel 的算子运行时编译，并把编译产物序列化进 om离线模型，后续可在模型执行阶段反序列化使用。
- 多 shape 说明：本样例主要提供按 shape key 管理和下沉多份 kernel binary 的多 shape / 多 kernel 处理框架。
- 编译期输入说明：样例在 `Compile` 阶段按 `Tensor` 布局读取输入，因此可以同时拿到 `Shape`、`DataType`、`Format` 等元信息；当前 key 仍然只按 shape 生成。

本样例展示一条最小可运行的 GE 自定义算子下沉链路：先构图生成 `AIR`，再通过 `ATC离线编译` 生成 `om离线模型`，最后由 `ACL` 程序加载并执行。样例中的 AddCustom 同时继承 `CompilableOp` 和 `PortableOp`，因此既能在 `Compile` 回调中通过 RTC 完成 Ascend C kernel 的算子运行时编译，也能把编译结果序列化到最终模型中。

## 适用场景

- 想看 `GE构图 + ATC离线编译 + om离线模型` 链路中的 Ascend C 自定义算子样例。
- 想参考自定义算子 RTC算子运行时编译、模型下沉和最终模型执行的完整流程。
- 想看 `AIR -> ATC离线编译 -> om离线模型 -> ACL` 这条最小可运行链路。
- 不适合用于了解 `PyTorch/TorchAir` 或 `TensorFlow/Triton` 场景。

## 前置依赖

### CANN

- 已正确安装并配置 CANN 环境，例如执行过 `source ${ASCEND_HOME_PATH}/set_env.sh`。
- `atc` 命令可用。
- 当前环境具备 `ACL`、`GE`、`Graph` 相关头文件与库。
- 参考 [安装指导](../../../docs/build.md#2-安装软件包) 完成 toolkit 和 ops 包安装。

### 框架与插件

- 本样例不依赖 PyTorch 或 TensorFlow。

### 环境变量

- `ASCEND_HOME_PATH`
- `ASCEND_CUSTOM_OPP_PATH` 会在 `run.sh` 中自动追加 `output/`，而自定义算子交付件会放到 `output/op_graph/lib/linux/x86_64/` 或 `output/op_graph/lib/linux/aarch64/`

### 额外依赖

- `cmake`
- `g++`

## 快速运行

在 `examples/custom_op/compilable_add_custom` 目录下执行：

### 推荐方式

```bash
bash run.sh
```

`run.sh` 在生成 `output/op_graph/lib/<os>/<arch>/libcust_opapi.so` 后会自动将 `output/` 追加到 `ASCEND_CUSTOM_OPP_PATH`。若运行成功，会生成 `output/single_add.air`、`output/single_add.om`，并在终端打印 `Model executed successfully!` 与 `First element of output: 2.000000`。

### 分步方式

如果想手动观察各阶段产物，可执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"

cd output
../build/compilable_add_graph_build
cd ..

atc \
  --model="$(pwd)/output/single_add.air" \
  --framework=1 \
  --output="$(pwd)/output/single_add" \
  --soc_version=Ascend910B1

cd build
./compilable_add_model_exec ../output/single_add.om
cd ..
```

其中 `export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"` 用于将自定义算子包根目录加入环境变量，随后 GE/ATC 会按 `output/op_graph/lib/<os>/<arch>/libcust_opapi.so` 规则加载交付件。

## 目录结构与关键文件

```text
compilable_add_custom
├── CMakeLists.txt
├── README.md
├── run.sh
├── ge
│   ├── add_custom_kernel.asc // 算子运行时编译时读取的 Ascend C kernel 源码
│   ├── add_custom.h          // AddCustom 的唯一 proto 定义，供交付件和构图侧共用
│   ├── custom_op.cpp         // CompilableOp/PortableOp 主流程实现
│   ├── infershape.cpp        // shape/type 推导注册逻辑
│   └── utils
│       ├── compile_utils.cpp           // 编译期输入 Tensor 读取、平台信息、kernel 路径和 shape key 等辅助逻辑
│       └── kernel_binary_map_utils.cpp // 多 bin 序列化/反序列化逻辑
├── graph_build
│   └── main.cc               // 构图并导出 single_add.air
└── model_exec
    └── main.cc               // 加载 om离线模型并执行
```

重点文件：

- `ge/custom_op.cpp`
  自定义算子的核心主流程，负责串起算子运行时编译、序列化、反序列化和执行；编译期按输入 Tensor 读取 `Shape/DataType/Format` 元信息。
- `ge/infershape.cpp`
  注册 `AddCustom` 的 shape/type 推导逻辑。
- `ge/add_custom.h`
  `AddCustom` 的唯一 proto 定义；构图侧使用生成到 `output/op_graph/include/` 下的同名头文件。
- `ge/add_custom_kernel.asc`
  算子运行时编译阶段读取的 Ascend C kernel 源码。
- `ge/utils/compile_utils.cpp`
  封装编译期输入 Tensor 读取、平台信息读取、kernel 源码路径定位，以及基于 shape 的 binary key 生成逻辑，用于演示多 shape 场景下的 binary 管理方式。
- `ge/utils/kernel_binary_map_utils.cpp`
  负责多份 kernel 二进制的序列化与反序列化，重点体现多 key / 多 binary 的落盘与恢复。
- `graph_build/main.cc`
  构建最小 Add 图并导出 `single_add.air`。
- `model_exec/main.cc`
  加载 om离线模型，准备输入输出并执行模型。
- `run.sh`
  串起 configure、build、install、ATC离线编译和模型执行。

## 核心链路

1. `ge/custom_op.cpp` 在 `Compile` 回调中先读取输入 Tensor 的 `Shape/DataType/Format` 等元信息，再读取 `ge/add_custom_kernel.asc`，通过 `aclrtc` 完成 Add kernel 的算子运行时编译。
2. `graph_build/main.cc` 构造最小 Add 图并导出 `single_add.air`。
3. `atc` 将 `single_add.air` 离线编译为 `single_add.om`。
4. `model_exec/main.cc` 通过 ACL 加载 `single_add.om` 并执行，输出第一项结果用于校验。

说明：样例中编译期输入已经按 Tensor 方式组织，因此可以直接拿到 shape、data type、format；当前仍然只用输入 shape 生成 key，并据此缓存、序列化和反序列化 kernel binary，整体上提供的是多 shape / 多 kernel 的处理框架。

## 构建产物

- `output/op_graph/lib/linux/x86_64/libcust_opapi.so`
  Linux x86_64 环境下 GE/ATC离线编译使用的自定义算子交付件；aarch64 环境对应 `output/op_graph/lib/linux/aarch64/libcust_opapi.so`。
- `output/op_graph/include/add_custom.h`
  构图侧可直接使用的 AddCustom proto 头文件，由构建阶段从 `ge/add_custom.h` 复制生成。
- `output/op_graph/lib/linux/x86_64/add_custom_kernel.asc`
  随交付件一起输出的 kernel 源码文件，供算子运行时编译阶段读取；aarch64 环境对应 `output/op_graph/lib/linux/aarch64/add_custom_kernel.asc`。
- `output/single_add.air`
  构图程序导出的 AIR 文件，作为 ATC离线编译输入。
- `output/single_add.om`
  ATC离线编译生成的 `.om` 离线模型文件。
- `build/compilable_add_graph_build`
  graph_build 程序。
- `build/compilable_add_model_exec`
  om离线模型执行程序。

## 结果校验

成功时可观察到：

- `output/single_add.air` 已生成。
- `output/single_add.om` 已生成。
- 终端输出包含 `Model executed successfully!`。
- 终端输出包含 `First element of output: 2.000000`。

若失败，优先检查：

- `ASCEND_HOME_PATH` 是否已设置并已正确 `source` CANN 环境。
- `atc` 是否可用。
- `output/op_graph/lib/<os>/<arch>/libcust_opapi.so`、`output/op_graph/lib/<os>/<arch>/add_custom_kernel.asc`
  以及 `output/op_graph/include/add_custom.h` 是否已生成。
- 当前 `soc_version` 是否与实际环境一致，`run.sh` 默认使用 `Ascend910B1`。

## 注意事项 / 限制

- `run.sh` 当前固定 `soc_version=Ascend910B1`，如需适配其他产品请按实际环境调整。
- `build/compilable_add_model_exec` 需要以位置参数传入模型路径。
- `run.sh` 会自动将 `ASCEND_CUSTOM_OPP_PATH` 追加为 `output/`，以便 `ATC离线编译` 按 `op_graph/lib/<os>/<arch>/` 目录规范加载 `libcust_opapi.so`。
- 当前样例按 shape key 组织多份 kernel binary，主要提供多 shape / 多 kernel 的处理框架。
- 当前 sample 依赖已安装的 CANN toolkit 头文件，并直接通过 `OpCompileContext::GetInputTensor` 读取编译期输入。

## 附录

### 参考文档

- RTC 算子运行时编译参考：[Ascend C 算子即时编译](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta2/opdevg/Ascendcopdevg/atlas_ascendc_10_00040.html)
- ATC 离线编译参考：[ATC 工具使用指南](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta2/devaids/atctool/atlasatc_16_0003.html)

### 算子运行时编译与下沉机制

- `CompilableOp` 负责在 `Compile` 回调中读取 kernel 源码并调用 `aclrtc` 完成算子运行时编译。
- `Compile` 阶段的输入读取已经按 `Tensor` 组织，因此除 shape 外还可以直接访问 data type 和 format。
- `PortableOp` 负责把编译产物序列化进最终模型，并在执行阶段反序列化恢复。
- 这两部分配合后，ATC离线编译生成的 om离线模型中会包含样例自定义算子运行所需的设备侧二进制；若后续扩展为 key 对应不同 kernel 的场景，本样例已给出多份 binary 的管理与下沉方式。

### 关键实现职责

- `Compile`：读取源码并触发算子运行时编译。
- `Serialize / Deserialize`：把编译产物写入模型并在执行阶段恢复。
- `Execute`：在模型执行阶段加载设备侧二进制并发起 kernel 调用。
