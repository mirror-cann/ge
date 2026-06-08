````# Ascend C 算子通过 TorchAir 入图样例

## 样例概述

- 构图入口：`PyTorch + TorchAir`
- 算子编成语言：`Ascend C`
- 编译方式：`预编译/直接调用`
- 模型下沉能力：`不涉及`
- 核心链路：`Ascend C kernel -> GE 交付件 -> TorchAir 入图 -> 图模式执行`
- 与其他 sample 的区别：本样例聚焦 `PyTorch + TorchAir` 场景，不涉及 `ATC离线编译 -> om离线模型` 模型下沉，也不涉及 Triton kernel。

本样例展示如何注册 Ascend C 自定义算子，并通过 `TorchAir` 将算子接入 GE 图。样例以 Add 算子为例，交付件侧通过 `EagerExecuteOp` 完成 GE 入图执行，Python 侧同时覆盖 eager 调用和图模式编译调用。

## 适用场景

- 想看 `PyTorch + TorchAir` 场景下 Ascend C 自定义算子的最小入图链路。
- 想参考 `TORCH_LIBRARY` 注册、自定义 converter 和 GE 自定义算子之间的配合方式。
- 想验证 Ascend C kernel 在 eager 和图模式下的数值结果。

## 前置依赖

### CANN

- 参考 [安装指导](../../../docs/build.md#2-安装软件包) 完成 toolkit 和 ops 包安装。

### 框架与插件

- 已安装 `PyTorch`、`torch_npu` 和 `TorchAir`。
- `TorchAir` 依赖的 Python 版本需满足其发布要求。

参考：

- [Ascend Extension for PyTorch](https://gitcode.com/Ascend/pytorch)
- [Ascend Extension for PyTorch 昇腾社区说明](https://hiascend.com/document/redirect/Pytorch-index)

### 环境变量

- `ASCEND_HOME_PATH`
- `LD_LIBRARY_PATH` 等 CANN 运行时相关变量

### 额外依赖

```bash
pip3 install expecttest
```

## 快速运行

在 `examples/custom_op/ascendc_add_custom` 目录下执行：

### 推荐方式

```bash
mkdir -p build
cd build
cmake ..
make -j"$(nproc)"
export ASCEND_CUSTOM_OPP_PATH="$(pwd):$ASCEND_CUSTOM_OPP_PATH"
python3 ../script/add_custom_test.py
```

若终端出现 `Ran 1 test` 和 `OK`，表示样例运行成功。

### 分步方式

1. 先执行 `cmake ..` 和 `make -j"$(nproc)"` 生成 `build/libcust_opapi.so`。
2. 再执行 `export ASCEND_CUSTOM_OPP_PATH="$(pwd):$ASCEND_CUSTOM_OPP_PATH"`，将 `libcust_opapi.so` 所在目录加入环境变量。
3. 最后执行 `python3 ../script/add_custom_test.py` 运行 eager 和 TorchAir 图模式测试。
4. 如需执行安装目标，可额外运行 `cmake --install build`。

## 目录结构与关键文件

```text
ascendc_add_custom
├── CMakeLists.txt
├── README.md
├── add_custom_kernel
│   ├── add_custom.asc        // Ascend C kernel 实现与 PyTorch 注册代码
│   ├── add_custom_kernel.h   // kernel 声明
│   └── custom_op.cpp         // GE 交付件，实现 Execute 入图逻辑
└── script
    └── add_custom_test.py    // Python 测试脚本，覆盖 eager 和图模式
```

重点文件：

- `add_custom_kernel/add_custom.asc`
  Ascend C Add kernel 的实现文件，同时包含 PyTorch 自定义算子注册逻辑。
- `add_custom_kernel/custom_op.cpp`
  自定义算子的 GE 交付件，实现 `Execute` 路径并完成 GE 入图。
- `script/add_custom_test.py`
  Python 测试脚本，覆盖 eager 执行和 TorchAir 图模式编译执行。
- `CMakeLists.txt`
  编译 `libcust_opapi.so` 并配置 PyTorch、TorchAir、CANN 依赖。

## 核心链路

1. 在 `add_custom_kernel/add_custom.asc` 中实现 Ascend C Add kernel，并完成 PyTorch 自定义算子注册。
2. 在 `add_custom_kernel/custom_op.cpp` 中实现 `EagerExecuteOp`，将自定义算子注册到 GE。
3. 通过 `script/add_custom_test.py` 加载 `libcust_opapi.so`，在 eager 与 TorchAir 图模式下执行并校验结果。

## 构建产物

- `build/libcust_opapi.so`
  GE/TorchAir 使用的自定义算子交付件。

## 结果校验

成功时可观察到：

- 终端输出包含 `Ran 1 test`。
- 终端输出包含 `OK`。
- 测试过程未出现 `Numerical values do not match within tolerance` 报错。

若失败，优先检查：

- `ASCEND_HOME_PATH`、`LD_LIBRARY_PATH` 等环境变量是否正确。
- `torch`、`torch_npu`、`TorchAir` 版本是否匹配。
- `build/libcust_opapi.so` 是否已成功生成并可被 Python 脚本加载。
- 当前环境是否具备可用 NPU，脚本会直接校验 `torch.npu.is_available()`。

## 注意事项 / 限制

- 支持的产品：`Atlas A2 训练系列产品 / Atlas A2 推理系列产品`。
- 样例当前使用 `float16` 的 `8 x 2048` 输入做结果校验。
- `script/add_custom_test.py` 依赖可用 NPU 环境，CPU 环境下无法通过。
- 本样例展示的是 `PyTorch + TorchAir` 入图路径，不涉及模型下沉到 om离线模型。

## 附录

### 算子规格

| 项目 | 内容 |
| --- | --- |
| 算子类型 | `AddCustom` |
| 输入 | `x`, `y` |
| 输出 | `z` |
| 输入 shape | `8 x 2048` |
| 输出 shape | `8 x 2048` |
| 数据类型 | `float16` |
| 格式 | `ND` |
| kernel 名称 | `add_custom` |

### 算子实现补充

Ascend C Add kernel 的执行过程可概括为三个步骤：

1. `CopyIn`：将输入从 Global Memory 搬运到片上 Local Memory。
2. `Compute`：对两个 LocalTensor 执行加法运算。
3. `CopyOut`：将计算结果搬回输出 Tensor 对应的 Global Memory。

### 注册机制补充

- `TORCH_LIBRARY_FRAGMENT` 用于定义 Python 侧可见的算子签名。
- `TORCH_LIBRARY_IMPL` 用于把算子实现绑定到 `PrivateUse1`、`XLA`、`Meta` 等 `DispatchKey`。
- `script/add_custom_test.py` 中的 `@torchair.register_fx_node_ge_converter(...)` 负责把 PyTorch 节点映射到 GE 自定义算子 `AddCustom`。
