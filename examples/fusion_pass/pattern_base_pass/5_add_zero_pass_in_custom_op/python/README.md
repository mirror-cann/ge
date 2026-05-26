# 样例使用指导

## 功能描述

本目录提供 `pattern_base_pass/5_add_zero_pass_in_custom_op` 的 **纯 Python** 版本示例，逻辑与 C++ 版本一致：

- `patterns()` 定义两个 pattern：
  - `Data + Const -> Identity -> AddCustom -> AddCustom`
  - `Data + Const -> Identity -> TensorMove -> AddCustom -> AddCustom`
- `meet_requirements()` 中读取匹配到的 `Const.value`，按 C++ sample 同样的规则判断零值
- `replacement()` 用 `AddCustom(input0, input1)` 替换匹配到的加零结构

## 目录结构

```
python/
├── README.md                     // Python 样例说明
├── CMakeLists.txt                // 生成 AddCustom Python ES API 的编译脚本
├── src
│   ├── python_addcustom_zero_pass.py // Python pass 实现文件
```

## 环境要求

- 临时要求：**run 包编译时使用的 Python 版本，需要与执行 sample 的 Python 版本保持一致**
- CANN 软件包安装请参考 [环境准备](../../../../../docs/build.md#1-环境准备)
- 环境变量设置请参考 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译)
- 已安装图编译流程相关 Python 依赖：`attrs`、`decorator`、`sympy`、`numpy`、`psutil`、`scipy`

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。
但 run 包不会内置本样例的 `AddCustom` Python ES API，需要先根据 `cpp/proto/add_custom_proto.cc` 生成 `es_custom` wheel 后再运行 Python pass。

## 准备工作

编写自定义算子的 Python pass 前，需要先完成自定义算子工程创建、算子实现、算子包编译部署以及算子适配开发，可参考 [C++ 样例准备工作](../cpp/README.md#准备工作)。

## 使用方式

1. 生成 `AddCustom` 的 Python ES API：

   ```bash
   cmake -S . -B build
   cmake --build build --target build_es_custom -j$(nproc)
   ```

   本目录的 `CMakeLists.txt` 复用 C++ 样例中的 `../cpp/proto/add_custom_proto.cc`，通过 `add_es_library_and_whl` 生成 `es_custom` 的 C/C++ 库和 Python wheel。

2. 安装生成的 Python 包，并让当前 Python 进程能找到包和对应的动态库：

   ```bash
   pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_custom-1.0.0-py3-none-any.whl
   export PYTHONPATH="$PWD/build/whl_package:${PYTHONPATH:-}"
   export LD_LIBRARY_PATH="$PWD/build/es_output/lib64:${LD_LIBRARY_PATH:-}"
   ```

3. 设置 Python pass 插件路径：

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_addcustom_zero_pass.py
   ```

4. 复用 [C++ pass 样例 README](../cpp/README.md#验证) 中的在线推理步骤执行模型编译。

5. 说明：

   - 该 sample 不是独立执行脚本，直接运行 `python src/python_addcustom_zero_pass.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印

## 预期日志

运行成功后，日志中会出现类似输出：

```text
Define pattern for PythonAddCustomZeroPass
Define MeetRequirements for PythonAddCustomZeroPass
[PythonAddCustomZeroPass] matched=addcustom_zero_pattern0 const_dtype=DT_FLOAT16 zero=True
Define replacement for PythonAddCustomZeroPass
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
- 已按上文生成并通过 `PYTHONPATH`、`LD_LIBRARY_PATH` 加载 `es_custom`
- `ge.es.custom.AddCustom`、`ge.es.math.Identity`、`ge.es.math.TensorMove` 可按需导入
- 再按 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译) 完成环境变量设置
- 最后按上文设置 `ASCEND_GE_PY_PASS_PATH` 并复用 C++ sample 的运行步骤
