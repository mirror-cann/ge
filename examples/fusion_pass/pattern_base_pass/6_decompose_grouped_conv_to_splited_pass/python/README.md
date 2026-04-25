## Python Pass[v1]

### 功能描述

本目录提供 [src/test_python_decompose_pass.py](./src/test_python_decompose_pass.py)，
演示如何使用纯 Python 编写 `DecomposePass`，把 `groups != 1` 且 `data_format == "NCHW"` 的 `Conv2D`
拆分为 `Split + groups 个 Conv2D + Concat`。

该 sample 对照 [C++ 样例](../cpp/README.md) 的主链路，已对齐以下行为：

- `op_types=["Conv2D"]`
- `meet_requirements()` 中检查 `groups != 1` 且 `data_format == "NCHW"`
- `replacement()` 中按 `Split(input) + Split(filter) + 多个 Conv2D + Concat` 构造 replacement graph

当前 sample 的 ES API 选择优先复用 run 包已提供的模块，而不是先自生成 `es_all`。
按当前环境中 run 包的实际导出情况，本 sample 采用：

- `Split` / `Concat` / `ConcatV2` 使用 run 包里的 `from ge.es.math import xx`
- 卷积接口使用 run 包里的 `from ge.es.nn import Conv2DV2`

如果需要查看这些接口的 Python 用法，可直接参考仓内现有 README：

- `Split`：[examples/es/dynamic_output/python/README.md](../../../../es/dynamic_output/python/README.md)
- `ConcatV2`：[examples/es/dynamic_input/python/README.md](../../../../es/dynamic_input/python/README.md)

当前仍有一组 Python helper 尚未补齐，因此它还不是和 C++ 样例完全等价的版本：

- `Node.get_input_desc`
- `Node.get_output_desc`
- `Node.update_input_desc`
- `Node.update_output_desc`
- `Node.get_input_const_tensor`
- `GeUtils.InferShape`
- `GeUtils.CheckNodeSupportOnAicore`

这意味着当前 Python sample 主要用于演示 `DecomposePass` 的注册方式、匹配条件和 replacement 拓扑；
像 C++ 样例那样在 `AfterInferShape` 阶段补 `TensorDesc`、执行 `InferShape`、再做 AICore 支持性校验，
还需要后续 Python API 补齐后才能完全对齐。

### 环境要求

- 临时要求：**run 包编译时使用的 Python 版本，需要与执行 sample 的 Python 版本保持一致**
- CANN 软件包安装请参考 [环境准备](../../../../../docs/build.md#1-环境准备)
- 环境变量设置请参考 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译)
- 已安装图编译流程相关 Python 依赖：`attrs`、`decorator`、`sympy`、`numpy`、`psutil`、`scipy`

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。
本 sample 当前使用的 `Split` / `Concat` / `ConcatV2` / `Conv2DV2` 都能直接从 run 包获得，
因此不再依赖本目录 `gen_es_api` 额外生成 `es_all-*.whl`。

### 使用方式

以下命令默认在 `6_decompose_grouped_conv_to_splited_pass` 目录执行。

1. 设置 Python pass 插件路径：

   ```bash
   export ASCEND_GE_PY_PASS_PATH="$PWD/python/src/test_python_decompose_pass.py"
   ```

2. 复用 [C++ 样例 README](../cpp/README.md#程序运行) 中已有的数据准备、ATC 离线编译或在线推理步骤执行模型编译。

3. 说明：

   - 该 sample 不是独立执行脚本，直接运行 `python python/src/test_python_decompose_pass.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印
   - 如果导入 `ge.es.math` 或 `ge.es.nn` 失败，通常表示当前环境变量未按 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#build-env) 正确设置，或 run 包 / ops 包未安装完整

### 预期日志

运行成功后，日志中会出现类似输出：

```text
Define MeetRequirements for DecomposeGroupedConvToSplitedPass
Define Replacement for DecomposeGroupedConvToSplitedPass
```

### Conda 环境示例（Python 3.11）

如果本机没有现成的匹配环境，可以参考下面的方式创建：

```bash
conda create -n ge-pass-py311 python=3.11 -y
conda activate ge-pass-py311
python -m pip install --upgrade pip
python -m pip install attrs decorator sympy numpy psutil scipy
```

创建环境后，请确认：

- 该环境中的 Python 版本与 run 包编译时使用的 Python 版本一致
- 再按 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#build-env) 完成环境变量设置
- 确认 `ge.es.math` 与 `ge.es.nn` 可直接导入
- 最后设置 `ASCEND_GE_PY_PASS_PATH` 并复用 C++ sample 的运行步骤
