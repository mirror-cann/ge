# DecomposeGroupedConvToSplitedPass Python 样例使用指导

## 功能描述

本目录提供 `pattern_base_pass/6_decompose_grouped_conv_to_splited_pass` 的 **纯 Python** 版本示例，逻辑与 C++ 版本一致：

- `op_types=["Conv2D"]`，只匹配原图中的 `Conv2D` 节点
- `meet_requirements()` 中检查 `groups != 1` 且 `data_format == "NCHW"`
- `replacement()` 中按 `Split(input) + Split(filter) + 多个 Conv2D + Concat` 构造 replacement graph
- 对 replacement graph 中每个 `Conv2D` 更新输入/输出 `TensorDesc`，设置 `FORMAT_NCHW` / `DT_FLOAT`
- 对 replacement graph 执行 `InferShape`，完成替换子图的 shape 推导

## 目录结构

```
python/
├── README.md                     // Python 样例说明
├── CMakeLists.txt                // 生成 Python ES API 的编译脚本
├── src
│   ├── python_decompose_pass.py  // Python pass 实现文件
```

## 前置条件

- 已完成 CANN 环境变量设置，设置方式为 `source ${ASCEND_PATH}/set_env.sh`，更多指导请参考 [C++ 样例 README](../cpp/README.md) 的配置环境变量步骤
- CANN 软件包安装请参考 [环境准备](../../../../../docs/build.md#1-环境准备)
- 已安装图编译流程相关 Python 依赖：`attrs`、`decorator`、`sympy`、`numpy`、`psutil`、`scipy`

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。
Python pass 运行时会加载基于 `pybind11` 的预编译二进制组件。CANN 包优先提供与当前 Python 版本匹配的产物；若无匹配产物，会自动进入 fallback 编译流程。fallback 编译需要当前 Python 环境中已安装 `pybind11`。
如果执行时提示缺少 `Conv2D` 等 ES API，再按下文“ES API 缺失时处理（可选）”生成并加载 `es_all`。

## 使用方式

以下命令默认在 `6_decompose_grouped_conv_to_splited_pass/python` 目录执行。

1. 设置 Python pass 插件路径：

   ```bash
   export ASCEND_GE_PY_PASS_PATH="$PWD/src/python_decompose_pass.py"
   ```

2. 参考 [C++ 样例 README](../cpp/README.md) 的程序运行章节进行验证。
   离线场景请将 C++ README 中的 `atc` 命令替换为 `pyatc`；两者命令行参数一致，`pyatc` 会在当前 Python 解释器进程中运行。

3. 说明：

   - 该样例不是独立执行脚本，直接运行 `python src/python_decompose_pass.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印
   - 如果执行时报缺少 `Conv2D` 等 ES API，先按下文生成并加载 `es_all`，再重新运行

## ES API 缺失时处理（可选）

如果执行时报缺少 `Conv2D` 等 ES API，可在 `6_decompose_grouped_conv_to_splited_pass/python` 目录生成 `es_all`：

```bash
cmake -S . -B build
cmake --build build --target build_es_all -j$(nproc)
```

安装生成的 Python 包，并让当前 Python 进程能找到包和对应的动态库：

```bash
pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_all-1.0.0-py3-none-any.whl
export PYTHONPATH="$PWD/build/whl_package:${PYTHONPATH:-}"
export LD_LIBRARY_PATH="$PWD/build/es_output/lib64:${LD_LIBRARY_PATH:-}"
```

## 预期日志

运行成功后，日志中会出现类似输出：

```text
Define MeetRequirements for PythonDecomposeGroupedConvToSplitedPass
Define Replacement for PythonDecomposeGroupedConvToSplitedPass
```
