# FuseMatMulAndAddPass Python 样例使用指导

本目录提供 `pattern_base_pass/1_fuse_matmul_add_pass` 的 **纯 Python** 版本示例，逻辑与 C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp) 一致：

- **Pattern 0**：`MatMul(a, b)` → `Add(..., c)`，图输入 `0/1/2` 对应 `a/b/c`
- **Pattern 1**：`BatchMatMulV2(a, b)` → `Add(..., c)`，同上三输入拓扑
- **Replacement**：`GEMM(r_a, r_b, r_c, alpha=1, beta=1)`（标量 `1.0` 对齐 C++ `CreateScalar(1)`）
- 继承 **`PatternFusionPass`**，使用 `@pattern` 方法声明 pattern、表达式 `replacement(self, inputs)` 声明替换图；阶段为 **`BeforeInferShape`**

未启用 `enable_const_value_match()` 等额外 matcher 配置，与 C++ 默认匹配行为一致。

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

run 包一般已包含 `ge_py` wheel，无需单独再装一份 `ge_py-*.whl`。
Python pass 运行时会加载基于 `pybind11` 的预编译二进制组件。CANN 包优先提供与当前 Python 版本匹配的产物；若无匹配产物，会自动进入 fallback 编译流程。fallback 编译需要当前 Python 环境中已安装 `pybind11`。
如果执行时提示缺少 `BatchMatMulV2`、`GEMM` 等 ES API，再按下文“ES API 缺失时处理（可选）”生成并加载 `es_all`。

## 使用方式

以下命令默认在 `1_fuse_matmul_add_pass/python` 目录执行。

1. 通过环境变量让 GE 在编译期加载该 Python pass：

```bash
export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_fuse_matmul_add_pass.py
```

2. 参考 [C++ 样例 README](../cpp/README.md) 的程序运行章节进行验证。

3. 说明：

   - 该样例不是独立执行脚本，直接运行 `python src/python_fuse_matmul_add_pass.py` 不会触发 pass 执行
   - 如果执行时报缺少 `BatchMatMulV2`、`GEMM` 等 ES API，先按下文生成并加载 `es_all`，再重新运行

## ES API 缺失时处理（可选）

如果执行时报缺少 `BatchMatMulV2`、`GEMM` 等 ES API，可通过本 Python 目录的 `CMakeLists.txt` 生成 `es_all`：

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

## 预期现象

与 C++ 样例类似，在 pattern / replacement 被调用时可看到：

```text
Define pattern for FuseMatMulAndAddPass
Define replacement for FuseMatMulAndAddPass
```

注意：由于使用了 `@pattern` 方法，日志中的 pattern graph name 为 `PythonFuseMatMulAndAddPass_matmul_add_pattern` 和 `PythonFuseMatMulAndAddPass_batch_matmul_add_pattern`。
