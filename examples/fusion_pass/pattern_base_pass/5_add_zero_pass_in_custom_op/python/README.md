# AddCustomZeroPass Python 样例使用指导

## 功能描述

本目录提供 `pattern_base_pass/5_add_zero_pass_in_custom_op` 的 **纯 Python** 版本示例，逻辑与 C++ 版本中一致：

- 使用 `@pattern` 方法定义两个 pattern：
  - `Data + Const -> Identity -> AddCustom -> AddCustom`
  - `Data + Const -> Identity -> TensorMove -> AddCustom -> AddCustom`
- `meet_requirements()` 中读取匹配到的 `Const.value`，按 C++ 样例同样的规则判断零值
- 表达式 `replacement(self, inputs)` 用 `AddCustom(inputs[0], inputs[1])` 替换匹配到的加零结构
- `@pattern` 方法通过 `inputs[0].get_owner_builder()` 获取框架的 `GraphBuilder`，使用 `create_const_float(0.0)` 显式创建 DT_FLOAT scalar Const 节点；框架自动设置图输出并 capture 输入

## 目录结构

```
python/
├── README.md                     // Python 样例说明
├── CMakeLists.txt                // 生成 AddCustom / es_all Python ES API 的编译脚本
├── src
│   ├── python_addcustom_zero_pass.py // Python pass 实现文件
```

## 前置条件

- 已完成 CANN 环境变量设置，设置方式为 `source ${ASCEND_PATH}/set_env.sh`，更多指导请参考 [C++ 样例 README](../cpp/README.md) 的配置环境变量步骤
- CANN 软件包安装请参考 [环境准备](../../../../../docs/zh/build.md#1-环境准备)
- 已安装图编译流程相关 Python 依赖：`attrs`、`decorator`、`sympy`、`numpy`、`psutil`、`scipy`

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。
Python pass 运行时会加载基于 `pybind11` 的预编译二进制组件。CANN 包优先提供与当前 Python 版本匹配的产物；若无匹配产物，会自动进入 fallback 编译流程。fallback 编译需要当前 Python 环境中已安装 `pybind11`。
但 run 包不会内置本样例的 `AddCustom` Python ES API，需要先根据 `cpp/proto/add_custom_proto.cc` 生成 `es_custom` wheel 后再运行 Python pass。

## ES API 说明

- `AddCustom` 来自本样例生成的 `es_custom` wheel，按下文使用方式第 1、2 步生成并加载
- `Identity`、`TensorMove` 通常来自 run 包中的内置 ES API；如果执行时报缺少这些接口，请先确认已完成 CANN 环境变量设置，必要时按下文“内置 ES API 缺失时处理（可选）”生成并加载 `es_all` 后重新执行

## 准备工作

编写自定义算子的 Python pass 前，需要先完成自定义算子工程创建、算子实现、算子包编译部署以及算子适配开发，可参考 [C++ 样例 README](../cpp/README.md) 的准备工作章节。

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

3. 内置 ES API 缺失时处理（可选）：

   如果执行时报缺少 `Identity`、`TensorMove` 等内置 ES API，可在完成上文 `cmake -S . -B build` 后额外生成并安装 `es_all`：

   ```bash
   cmake --build build --target build_es_all -j$(nproc)
   pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_all-1.0.0-py3-none-any.whl
   ```

   `PYTHONPATH` 与 `LD_LIBRARY_PATH` 复用上一步设置。

4. 设置 Python pass 插件路径：

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_addcustom_zero_pass.py
   ```

5. 参考 [C++ 样例 README](../cpp/README.md) 的验证章节进行验证。

6. 说明：

   - 该样例不是独立执行脚本，直接运行 `python src/python_addcustom_zero_pass.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印

## 预期日志

运行成功后，日志中会出现类似输出：

```text
Define pattern for PythonAddCustomZeroPass
Define MeetRequirements for PythonAddCustomZeroPass
[PythonAddCustomZeroPass] matched=PythonAddCustomZeroPass_addcustom_zero_pattern const_dtype=DT_FLOAT16 zero=True
Define replacement for PythonAddCustomZeroPass
```
