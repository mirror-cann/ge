# FuseMatMulAndAddPass（pattern matcher config）Python 样例使用指导

## 功能描述

本目录提供 `pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config` 的 **纯 Python** 版本示例，逻辑与 C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp) 一致。

- 在构造函数中启用：
  - `PatternMatcherConfigBuilder().enable_const_value_match()`
  - `PatternMatcherConfigBuilder().enable_ir_attr_match()`
- `patterns()` 定义 `MatMul(x, y, transpose_x1=False, transpose_x2=False) + Add(Const)` 拓扑。
- `replacement()` 定义 `GEMM(x, y, Const, alpha=1, beta=1)`。
- 注册阶段为 `PassStage.BEFORE_INFER_SHAPE`。

## 目录结构

```
python/
├── README.md                     // Python 样例说明
├── CMakeLists.txt                // 生成 es_all Python ES API 的编译脚本
├── src
│   ├── python_fuse_matmul_add_pass.py // Python pass 实现文件
```

## 前置条件

- 已完成 CANN 环境变量设置，设置方式为 `source ${ASCEND_PATH}/set_env.sh`，更多指导请参考 [C++ 样例 README](../cpp/README.md) 的配置环境变量步骤
- 可导入 GE Python 包（含 `ge.passes` 与 pass 加载链路）

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。
Python pass 运行时会加载基于 `pybind11` 的预编译二进制组件。CANN 包优先提供与当前 Python 版本匹配的产物；若无匹配产物，会自动进入 fallback 编译流程。fallback 编译需要当前 Python 环境中已安装 `pybind11`。
如果执行时提示缺少 `GEMM` 等 ES API，再按下文“ES API 缺失时处理（可选）”生成并加载 `es_all`。

## 使用方式

以下命令默认在 `3_fuse_matmul_add_pass_with_pattern_matcher_config/python` 目录执行。

1. 通过环境变量让 GE 在编译期加载该 Python pass：

```bash
export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_fuse_matmul_add_pass.py
```

2. 参考 [C++ 样例 README](../cpp/README.md) 的程序运行章节进行验证。

3. 说明：

   - 该样例不是独立执行脚本，直接运行 `python src/python_fuse_matmul_add_pass.py` 不会触发 pass 执行
   - 预期输出会在 GE 编译流程真正加载该 Python pass 后打印
   - 如果执行时报缺少 `GEMM` 等 ES API，先按下文生成并加载 `es_all`，再重新运行

## ES API 缺失时处理（可选）

如果执行时报缺少 `GEMM` 等 ES API，可通过本 Python 目录的 `CMakeLists.txt` 生成 `es_all`：

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

在匹配与替换发生时，日志中会出现类似输出：

```text
Define pattern for MatmulAddFusionPass in matcher config sample
Define replacement for MatmulAddFusionPass in matcher config sample
```

对于 `es_forward_2.py` 与 `es_forward_3.py` 这类故意构造的未命中场景，通常仅看到 pattern 定义日志而不会进入 replacement。
