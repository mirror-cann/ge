# 样例使用指导

## 功能描述

本目录提供 `pattern_base_pass/6_decompose_grouped_conv_to_splited_pass` 的 **纯 Python** 版本示例，逻辑与 C++ 版本一致：

- `op_types=["Conv2D"]`，只匹配原图中的 `Conv2D` 节点
- `meet_requirements()` 中检查 `groups != 1` 且 `data_format == "NCHW"`
- `replacement()` 中按 `Split(input) + Split(filter) + 多个 Conv2D + Concat` 构造 replacement graph
- 对 replacement graph 中每个 `Conv2D` 更新输入/输出 `TensorDesc`，设置 `FORMAT_NCHW` / `DT_FLOAT`
- 对 replacement graph 执行 `InferShape`，并对非 `Data` / `Const` 节点做 AICore 支持性校验

## 目录结构

```
python/
├── README.md                     // Python 样例说明
├── CMakeLists.txt                // 生成 Python ES API 的编译脚本
├── src
│   ├── python_decompose_pass.py  // Python pass 实现文件
```

## 环境要求

- 临时要求：**run 包编译时使用的 Python 版本，需要与执行 sample 的 Python 版本保持一致**
- CANN 软件包安装请参考 [环境准备](../../../../../docs/build.md#1-环境准备)
- 环境变量设置请参考 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译)
- 已安装图编译流程相关 Python 依赖：`attrs`、`decorator`、`sympy`、`numpy`、`psutil`、`scipy`

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。
但本 sample 需要先生成并加载 `es_all`，以获得与 C++ replacement graph 一致的 `Conv2D` Python ES API。

## 使用方式

以下命令默认在 `6_decompose_grouped_conv_to_splited_pass` 目录执行。

1. 生成 `es_all` Python ES API：

   ```bash
   cmake -S . -B build
   cmake --build build --target build_es_all -j$(nproc)
   ```

2. 安装生成的 Python 包，并让当前 Python 进程能找到包和对应的动态库：

   ```bash
   pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_all-1.0.0-py3-none-any.whl
   export PYTHONPATH="$PWD/build/whl_package:${PYTHONPATH:-}"
   export LD_LIBRARY_PATH="$PWD/build/es_output/lib64:${LD_LIBRARY_PATH:-}"
   ```

3. 设置 Python pass 插件路径：

   ```bash
   export ASCEND_GE_PY_PASS_PATH="$PWD/src/python_decompose_pass.py"
   ```

4. 复用 [C++ pass 样例 README](../cpp/README.md) 中的程序编译+程序执行进行验证。
   离线场景请将 C++ README 中的 `atc` 命令替换为 `pyatc`；两者命令行参数一致，`pyatc` 会在当前 Python 解释器进程中运行。

5. 说明：

   - 该 sample 不是独立执行脚本，直接运行 `python src/python_decompose_pass.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印
   - 如果导入 `ge.es.all` 失败，通常表示 `es_all` 未生成或 `PYTHONPATH`、`LD_LIBRARY_PATH` 未按上文设置

## 预期日志

运行成功后，日志中会出现类似输出：

```text
Define MeetRequirements for DecomposeGroupedConvToSplitedPass
Define Replacement for DecomposeGroupedConvToSplitedPass
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
- 再按 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#build-env) 完成环境变量设置
- 已按上文生成并通过 `PYTHONPATH`、`LD_LIBRARY_PATH` 加载 `es_all`
- 确认 `ge.es.all` 可直接导入
- 最后设置 `ASCEND_GE_PY_PASS_PATH` 并复用 C++ sample 的运行步骤
