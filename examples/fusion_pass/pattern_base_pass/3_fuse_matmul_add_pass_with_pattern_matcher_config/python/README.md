# Python Pass[v1]

## 功能描述

本目录提供 `pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config` 的 **纯 Python** 版本示例，逻辑与 C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp) 一致。


- 在构造函数中启用：
  - `PatternMatcherConfigBuilder().enable_const_value_match()`
  - `PatternMatcherConfigBuilder().enable_ir_attr_match()`
- `Patterns` 定义 `MatMul(x, y, transpose_x1=False, transpose_x2=False) + Add(Const)` 拓扑。
- `Replacement` 定义 `GEMM(x, y, Const, alpha=1, beta=1)`。
- 注册阶段为 `PassStage.BEFORE_INFER_SHAPE`。


## 前置条件

- 已 source CANN 环境（`source ${ASCEND_PATH}/set_env.sh`）
- 临时要求：**run 包编译时使用的 Python 版本，需要与执行 sample 的 Python 版本保持一致**
- 环境变量设置请参考 [C++ 版本 README 的程序编译](../cpp/README.md#程序编译a-namesection6645633456813a)
- 可导入 GE Python 包（含 `ge.passes` 与 pass 加载链路）

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。

## 使用方式

1. 通过环境变量让 GE 在编译期加载该 Python pass（在 `3_fuse_matmul_add_pass_with_pattern_matcher_config/python` 目录下时）：

```bash
export ASCEND_GE_PY_PASS_PATH=$PWD/python/src/python_fuse_matmul_add_pass.py
```

2. 复用 [C++ pass 样例 README](../cpp/README.md) 中的程序编译+程序执行进行验证。


3. 说明：

- 该 sample 不是独立执行脚本，直接运行 `python python/src/python_fuse_matmul_add_pass.py` 不会触发 pass 执行
- 预期输出会在 GE 编译流程真正加载该 Python pass 后打印

## 预期日志

在匹配与替换发生时，日志中会出现类似输出：

```text
Define pattern for MatmulAddFusionPass in matcher config sample
Define replacement for MatmulAddFusionPass in matcher config sample
```

对于 `es_forward_2.py` 与 `es_forward_3.py` 这类故意构造的未命中场景，通常仅看到 pattern 定义日志而不会进入 replacement。
