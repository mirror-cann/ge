# Triton 自定义算子通过 TensorFlow 入图样例

## 样例概述

- 构图入口：`TensorFlow`
- 算子编成语言：`Triton`
- 编译方式：`预编译为 npubin`
- 模型下沉能力：`不涉及`
- 核心链路：`Triton kernel -> TensorFlow 交付件 + GE 交付件 -> TensorFlow 入图执行`
- 与其他 sample 的区别：本样例聚焦 Triton kernel 与 TensorFlow/GE 交付件协同，不涉及 TorchAir，也不涉及 `ATC离线编译 -> om离线模型` 模型下沉。

本样例展示如何把 Triton Add kernel 接入 TensorFlow，并通过 GE 自定义算子交付件完成 NPU 执行。链路包含 `npubin` 生成、TensorFlow 自定义算子 so 构建、GE 交付件构建，以及最终的 TensorFlow 脚本结果校验。

## 适用场景

- 想看 `TensorFlow + Triton` 场景下的自定义算子入图示例。
- 想参考 `npubin`、TensorFlow 交付件和 GE 交付件之间的配合方式。
- 想验证 Triton kernel 在 TensorFlow 图中的数值正确性。
- 不适合用于了解 `TorchAir` 或 `ATC离线编译 -> om离线模型` 链路。

## 前置依赖

### CANN

- 已安装并配置 CANN toolkit 和 ops 包。
- `ASCEND_HOME_PATH` 已设置。
- 参考 [安装指导](../../../docs/build.md#2-安装软件包) 完成 toolkit 和 ops 包安装。

### 框架与插件

- 已安装 `Triton-Ascend`。
- 已安装 `TensorFlow 1.15` 或 `TensorFlow 2.6.5` 及其对应框架插件包。

参考：

- [Triton-Ascend 安装说明](https://gitcode.com/Ascend/triton-ascend/blob/main/docs/zh/installation_guide.md)
- [Triton-Ascend 快速开始](https://gitcode.com/Ascend/triton-ascend/blob/main/docs/zh/quick_start.md)
- [TensorFlow 1.15 迁移说明](https://www.hiascend.com/document/detail/zh/TensorFlowCommunity/850/migration/tfmigr1/tfmigr1_000008.html)
- [TensorFlow 2.6.5 迁移说明](https://www.hiascend.com/document/detail/zh/TensorFlowCommunity/850/migration/tfmigr2/tfmigr2_000007.html)

### 环境变量

- `ASCEND_HOME_PATH`
- `ASCEND_CUSTOM_OPP_PATH`
- 如需自定义 `npubin` 输出目录，可设置 `TRITON_CACHE_DIR`

### 额外依赖

- `python3`
- `g++`
- `cmake`

## 快速运行

在 `examples/custom_op/triton_add_custom` 目录下执行以下最短路径：

### 推荐方式

```bash
cd add_custom_kernel
python3 add_custom_kernel.py

cd ../tensorflow
bash build_tf.sh

cd ../ge
bash run.sh
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/build_out:$ASCEND_CUSTOM_OPP_PATH"

cd ../tensorflow
python3 ../script/run_add_custom_tf_1.15.py
```

执行前请确认 `ge/src/custom_op.cpp` 中的 `bin_path` 能访问步骤 1 生成的 `add_kernel.npubin`。若运行成功，终端会打印 `The result of tf and ac is the same.`。

### 分步方式

1. 先在 `add_custom_kernel/` 下执行 `python3 add_custom_kernel.py` 生成 `npubin`。
2. 再在 `tensorflow/` 下执行 `bash build_tf.sh` 生成 `outputs/libcustom_ops.so`。
3. 接着在 `ge/` 下执行 `bash run.sh`，生成 `build_out/libcust_opapi.so` 和 `framework/tensorflow/npu_supported_ops.json`。
4. 执行 `export ASCEND_CUSTOM_OPP_PATH="$(pwd)/build_out:$ASCEND_CUSTOM_OPP_PATH"`，将 `libcust_opapi.so` 所在目录加入环境变量。
5. 最后回到 `tensorflow/` 目录执行 `python3 ../script/run_add_custom_tf_1.15.py`。

若需要调整 `npubin` 输出位置，可先设置 `TRITON_CACHE_DIR`；若需要重新指定 `npubin` 路径，可按实际缓存位置修改 `ge/src/custom_op.cpp` 中的 `bin_path`。

## 目录结构与关键文件

```text
triton_add_custom
├── README.md
├── add_custom_kernel
│   └── add_custom_kernel.py        // Triton Add kernel 实现与编译入口
├── ge
│   ├── CMakeLists.txt              // GE 交付件构建脚本
│   ├── gen_npu_supported_ops_json.sh
│   ├── run.sh
│   └── src
│       └── custom_op.cpp           // Triton 算子入 GE 交付件
├── tensorflow
│   ├── add_custom_triton_tf.cc     // TensorFlow 侧自定义算子声明
│   └── build_tf.sh                 // TensorFlow 交付件构建脚本
├── script
│   └── run_add_custom_tf_1.15.py   // TensorFlow 测试脚本
├── static_shape.png
└── dynamic_shape.png
```

重点文件：

- `add_custom_kernel/add_custom_kernel.py`
  Triton Add kernel 的实现与编译入口。
- `tensorflow/add_custom_triton_tf.cc`
  TensorFlow 侧自定义算子声明。
- `tensorflow/build_tf.sh`
  编译 `libcustom_ops.so`。
- `ge/src/custom_op.cpp`
  GE 自定义算子交付件，实现加载 `npubin` 并发起 kernel 执行。
- `ge/CMakeLists.txt`
  构建 `libcust_opapi.so` 并生成 `npu_supported_ops.json`。
- `ge/run.sh`
  GE 子步骤的一键脚本，负责 configure、build 和 install。
- `script/run_add_custom_tf_1.15.py`
  TensorFlow 图执行脚本，负责数值一致性校验。

## 核心链路

1. `add_custom_kernel/add_custom_kernel.py` 负责生成 Triton kernel 对应的 `npubin`。
2. `tensorflow/build_tf.sh` 构建 TensorFlow 侧 `libcustom_ops.so`。
3. `ge/CMakeLists.txt` 构建 GE 交付件 `libcust_opapi.so`，并生成 `npu_supported_ops.json` 供TensorFlow使用。
4. `script/run_add_custom_tf_1.15.py` 加载 TensorFlow 侧 so，并通过 TensorFlow 图执行自定义 AddCustom 算子。

## 构建产物

- `tensorflow/outputs/libcustom_ops.so`
  TensorFlow 侧自定义算子交付件。
- `ge/build_out/libcust_opapi.so`
  GE 侧自定义算子交付件。
- `ge/build_out/framework/tensorflow/npu_supported_ops.json`
  TensorFlow Adapter 加载自定义算子支持信息时使用的描述文件。
- `add_kernel.npubin`
  Triton kernel 编译后的二进制文件，默认由 Triton 写入缓存目录，路径可通过 `TRITON_CACHE_DIR` 控制。

## 结果校验

成功时可观察到：

- `tensorflow/outputs/libcustom_ops.so` 已生成。
- `ge/build_out/libcust_opapi.so` 已生成。
- `ge/build_out/framework/tensorflow/npu_supported_ops.json` 已生成。
- 终端输出包含 `The result of tf and ac is the same.`。

若失败，优先检查：

- `ge/src/custom_op.cpp` 中的 `bin_path` 是否能正确访问生成的 `npubin`。
- `ASCEND_CUSTOM_OPP_PATH` 是否已包含 `ge/build_out`。
- 测试脚本启动目录下是否能访问 `./outputs/libcustom_ops.so`。
- TensorFlow、框架插件包和 Triton-Ascend 安装是否匹配。

## 注意事项 / 限制

- 当前样例依赖 Triton-Ascend 对目标硬件的支持范围，请参考 [Triton-Ascend 项目说明](https://gitcode.com/Ascend/triton-ascend#硬件支持)。
- `ge/src/custom_op.cpp` 中的 `bin_path` 需要与实际生成的 `npubin` 路径保持一致。
- 测试脚本默认从当前工作目录的 `./outputs/libcustom_ops.so` 加载 TensorFlow 侧 so，建议在 `tensorflow/` 目录下运行测试脚本。
- 样例默认使用静态 shape；如需动态 shape，可在测试脚本中打开 `compile_dynamic_mode` 配置。

## 附录

### 动态 / 静态 shape 调测开关

在 `script/run_add_custom_tf_1.15.py` 中可通过以下配置启用动态 shape：

```python
custom_op.parameter_map["compile_dynamic_mode"].b = True
```

### Profiling 开关

```python
custom_op.parameter_map["profiling_mode"].b = True
custom_op.parameter_map["profiling_options"].s = tf.compat.as_bytes(
    '{"output":".","training_trace":"on","task_trace":"on","hccl":"on","aicpu":"on","aic_metrics":"PipeUtilization","msproftx":"off"}'
)
```

导出 profiling 数据后，可使用如下命令查看结果目录：

```bash
msprof --export=on --output=${profiling_path}
cd ${profiling_path}/mindstudio_profiler_output
```

然后找到 `msprof_*.json` 并使用如 `chrome://tracing/` 的工具加载。

### Profiling 现象说明

- 静态 shape 结果示意：![static_shape.png](./static_shape.png)
- 动态 shape 结果示意：![dynamic_shape.png](./dynamic_shape.png)

静态 shape 下可观测到 `MODEL_EXECUTE`、`EVENT_WAIT`、`MEMCPY_ASYNC` 等流同步和数据传输过程；动态 shape 下这些现象会有所不同。
