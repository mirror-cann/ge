# AddZeroPass Python 样例使用指导

## 功能描述

本目录提供两个纯 Python 的 `PatternFusionPass` 样例：

- [src/python_add_zero_pass_const_value_match.py](./src/python_add_zero_pass_const_value_match.py)
- [src/python_add_zero_pass.py](./src/python_add_zero_pass.py)

两个样例的定位不同：

- `python_add_zero_pass_const_value_match.py`
  - 演示 Python V1 的 `PatternMatcherConfigBuilder.enable_const_value_match()`
  - 通过 strict const-value-match 直接把 `Add(x, 0.0f)` 前置到 matcher 阶段
  - `@pattern` 方法使用 Python 表达式描述严格匹配的 `Add(x, 0.0f)`，例如 `return inputs[0] + 0.0`
  - 当前 `ConstantMatcher::IsMatch` 的值匹配是严格二进制匹配，不带浮点容差，也不做跨 dtype 归一化
  - 因此它更适合作为 matcher_config 示例，而不是 C++ 样例的完全等价版本

- `python_add_zero_pass.py`
  - 主逻辑对齐 [C++ pass 样例](../cpp/README.md)
  - `@pattern` 方法使用 Python 表达式描述 `Data + Const` 拓扑，例如 `return inputs[0] + 0`
  - 多输入 pattern 可使用 `x, y, z = inputs[:3]` 显式声明输入数量
  - 多 pattern pass 可声明多个 `@pattern` 方法；旧的 `patterns(self)` 返回多个 `Pattern` / `Graph` 仍兼容
  - `@pattern` 方法不能和 `patterns(self)` 同时使用，避免 pattern 声明来源不明确
  - Python pass 框架会自动创建 `GraphBuilder`、设置图输出并 capture 已使用的输入；旧的显式 `GraphBuilder` 写法仍兼容
  - `meet_requirements()` 中再显式读取匹配到的 `Const.value`，按 C++ 样例同样的规则判断零值
  - 当前支持与 C++ 样例一致的 `DT_FLOAT`、`DT_DOUBLE`、`DT_INT32`

表达式 pattern 的推荐写法如下：

```python
from ge.passes import PatternFusionPass, pattern


class PythonAddZeroPass(PatternFusionPass):
    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0

    def replacement(self, inputs):
        return inputs[0]
```

## 目录结构

```
python/
├── README.md                     // Python 样例说明
├── CMakeLists.txt                // 生成 es_all Python ES API 的编译脚本
├── src
│   ├── python_add_zero_pass.py   // Python pass 实现文件
│   ├── python_add_zero_pass_const_value_match.py // const value match 示例
```

## 前置条件

- 已完成 CANN 环境变量设置，设置方式为 `source ${ASCEND_PATH}/set_env.sh`，更多指导请参考 [C++ 样例 README](../cpp/README.md) 的配置环境变量步骤
- **run 包编译使用的 Python 版本**与执行本样例的 Python 版本一致。原因是 Python pass 相关扩展模块等与编译时 Python ABI 相关，版本不一致时可能导入失败或加载异常
- CANN 软件包安装请参考 [环境准备](../../../../../docs/build.md#1-环境准备)
- 已安装图编译流程相关 Python 依赖：`attrs`、`decorator`、`sympy`、`numpy`、`psutil`、`scipy`

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。

## 使用方式

1. 设置 Python pass 插件路径：

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_add_zero_pass_const_value_match.py
   ```

   如果需要运行与 C++ 主逻辑对齐的版本，可改为：

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_add_zero_pass.py
   ```

2. 参考 [C++ 样例 README](../cpp/README.md) 的程序运行章节进行验证。
   离线场景请将 C++ README 中的 `atc` 命令替换为 `pyatc`；两者命令行参数一致，`pyatc` 会在当前 Python 解释器进程中运行。

3. 说明：

   - 这两个样例都不是独立执行脚本，直接运行 `python src/python_add_zero_pass.py` 或 `python src/python_add_zero_pass_const_value_match.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印

## 预期日志

运行成功后，日志中会出现类似输出(input_0名字实际可能有区别)：

```text
[PythonAddZeroConstValueMatchPass] matched=PythonAddZeroConstValueMatchPass_add_zero_pattern captured=input_0:0
[PythonAddZeroPass] matched=PythonAddZeroPass_add_zero_pattern captured=input_0:0 const_dtype=DT_FLOAT zero=True
```

## Conda 环境示例（Python 3.11）

如果本机没有现成的匹配环境，可以参考下面的方式创建：

```bash
conda create -n ge-pass-py311 python=3.11 -y
conda activate ge-pass-py311
python -m pip install --upgrade pip
python -m pip install attrs decorator sympy numpy psutil scipy
```

创建环境后，请确认：

- 该环境中的 Python 版本与 run 包编译时使用的 Python 版本一致
- 再按 [C++ 样例 README](../cpp/README.md) 的配置环境变量步骤，完成环境变量设置
- 最后按本文使用方式设置 `ASCEND_GE_PY_PASS_PATH` 并参考 C++ 样例的运行步骤
