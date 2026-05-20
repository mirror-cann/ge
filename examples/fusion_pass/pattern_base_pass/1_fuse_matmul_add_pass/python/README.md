# FuseMatMulAndAddPass Python 样例使用指导

本目录提供 `pattern_base_pass/1_fuse_matmul_add_pass` 的 **纯 Python** 版本示例，逻辑与 C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp) 一致：~~

- **Pattern 0**：`MatMul(a, b)` → `Add(..., c)`，图输入 `0/1/2` 对应 `a/b/c`
- **Pattern 1**：`BatchMatMulV2(a, b)` → `Add(..., c)`，同上三输入拓扑
- **Replacement**：`GEMM(r_a, r_b, r_c, alpha=1, beta=1)`（标量 `1.0` 对齐 C++ `CreateScalar(1)`）
- 继承 **`PatternFusionPass`**，实现 `patterns()` / `meet_requirements()` / `replacement()`；阶段为 **`BeforeInferShape`**

未启用 `enable_const_value_match()` 等额外 matcher 配置，与 C++ 默认匹配行为一致。

## 前置条件

- 已 source CANN 环境（`source ${ASCEND_PATH}/set_env.sh`）
- **run 包编译使用的 Python 版本**与执行本样例的 Python 版本一致
- 环境变量设置请参考 [C++ 样例 README 的程序编译-配置环境变量](../cpp/README.md#程序编译a-namesection6645633456813a)
- Python 可导入 ES API（通常来自 run 包）：`MatMul`、`Add`、`GEMM`（`ge.es.math` 或 `ge.es.all`）、`BatchMatMulV2`（`ge.es.nn` 或 `ge.es.all`）
- 可导入 GE Python 包（含 `ge.passes` 与 pass 加载链路）

run 包一般已包含 `ge_py` wheel，无需单独再装一份 `ge_py-*.whl`。

## 使用方式

1. 通过环境变量让 GE 在编译期加载该 Python pass（在 `1_fuse_matmul_add_pass/python` 目录下时）：

```bash
export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_fuse_matmul_add_pass.py
```

2. 复用 [C++ pass 样例 README](../cpp/README.md) 中的程序编译+程序执行进行验证。

## 预期现象

与 C++ 样例类似，在 pattern / replacement 被调用时可看到：

```text
Define pattern for FuseMatMulAndAddPass
Define replacement for FuseMatMulAndAddPass
```

直接运行 `python src/python_fuse_matmul_add_pass.py` 只会打印注册提示，**不会**触发 GE 编译流程中的 pass 执行。
