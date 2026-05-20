# 样例使用指导

### 功能描述

本目录提供两个纯 Python 的 `PatternFusionPass` sample：

- [src/python_add_zero_pass_const_value_match.py](./src/python_add_zero_pass_const_value_match.py)
- [src/python_add_zero_pass.py](./src/python_add_zero_pass.py)

两个 sample 的定位不同：

- `python_add_zero_pass_const_value_match.py`
  - 演示 Python V1 的 `PatternMatcherConfigBuilder.enable_const_value_match()`
  - 通过 strict const-value-match 直接把 `Add(x, 0.0f)` 前置到 matcher 阶段
  - 当前 `ConstantMatcher::IsMatch` 的值匹配是严格二进制匹配，不带浮点容差，也不做跨 dtype 归一化
  - 因此它更适合作为 matcher_config 示例，而不是 C++ sample 的完全等价版本

- `python_add_zero_pass.py`
  - 主逻辑对齐 [C++ pass 样例](../cpp/README.md)
  - `patterns()` 只描述 `Data + Const` 拓扑
  - `meet_requirements()` 中再显式读取匹配到的 `Const.value`，按 C++ sample 同样的规则判断零值
  - 当前支持与 C++ sample 一致的 `DT_FLOAT`、`DT_DOUBLE`、`DT_INT32`

### 环境要求

- 临时要求：**run 包编译时使用的 Python 版本，需要与执行 sample 的 Python 版本保持一致**
- CANN 软件包安装请参考 [环境准备](../../../../../docs/build.md#1-环境准备)
- 环境变量设置请参考 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译)
- 已安装图编译流程相关 Python 依赖：`attrs`、`decorator`、`sympy`、`numpy`、`psutil`、`scipy`

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。

### 使用方式

1. 设置 Python pass 插件路径：

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_add_zero_pass_const_value_match.py
   ```

   如果需要运行与 C++ 主逻辑对齐的版本，可改为：

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_add_zero_pass.py
   ```

2. 复用 [C++ pass 样例 README](../cpp/README.md) 中的程序编译+程序执行进行验证。
   离线场景请将 C++ README 中的 `atc` 命令替换为 `pyatc`；两者命令行参数一致，`pyatc` 会在当前 Python 解释器进程中运行。

3. 说明：

   - 这两个 sample 都不是独立执行脚本，直接运行 `python src/python_add_zero_pass.py` 或 `python src/python_add_zero_pass_const_value_match.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印

### 预期日志

运行成功后，日志中会出现类似输出：

```text
[PythonAddZeroConstValueMatchPass] matched=add_zero_const_value_match_pattern captured=input_0:0
[PythonAddZeroPass] matched=add_zero_pattern captured=input_0:0 const_dtype=DT_FLOAT zero=True
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
- 再按 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译) 完成环境变量设置
- 最后按上文设置 `ASCEND_GE_PY_PASS_PATH` 并复用 C++ sample 的运行步骤
