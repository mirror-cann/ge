## Python Pass[v1]

### 功能描述

本目录提供 [src/test_python_pass.py](./src/test_python_pass.py)，
演示如何使用纯 Python 编写 `FusionBasePass` 并通过 `@register_fusion_pass` 注册。
sample 会在 GE 编译流程中打印 `PassContext` 与 `Graph` 信息，并演示图属性的读写。

该 sample 对应 [C++ 版本样例](../cpp/README.md)，使用方式是：

- Python pass 文件通过 `ASCEND_GE_PY_PASS_PATH` 被 GE 编译流程发现
- 模型生成、ATC 离线编译、在线推理步骤继续复用 C++ README 中已有流程

### 环境要求

- 临时要求：**run 包编译时使用的 Python 版本，需要与执行 sample 的 Python 版本保持一致**
- CANN 软件包安装请参考 [环境准备](../../../../../docs/build.md#1-环境准备)
- 环境变量设置请参考 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译a-namesection6645633456813a)
- 已安装图编译流程相关 Python 依赖：`attrs`、`decorator`、`sympy`、`numpy`、`psutil`、`scipy`

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。

### 使用方式

1. 设置 Python pass 插件路径：

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/test_python_pass.py
   ```

2. 复用 [C++ pass 样例 README](../cpp/README.md#程序运行) 中的模型生成、ATC 或在线推理步骤执行编译。

3. 说明：

   - 该 sample 不是独立执行脚本，直接运行 `python src/test_python_pass.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印

### 预期日志

运行成功后，日志中会出现类似输出：

```text
--------PassContext as follow--------
<ge.passes.ge_pass_native.PassContext object at 0x...>
TestPass
halo, i'm testpass
-----------Graph as follow-----------
graph("model"):
  ...
a test attr
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
- 再按 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译a-namesection6645633456813a) 完成环境变量设置
- 最后按上文设置 `ASCEND_GE_PY_PASS_PATH` 并复用 C++ sample 的运行步骤
