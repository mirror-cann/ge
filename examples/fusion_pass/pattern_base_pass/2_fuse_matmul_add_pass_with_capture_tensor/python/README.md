# FuseMatMulAndAddPass（capture tensor）Python 样例使用指导

本目录提供 `pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor` 的 **纯 Python** 版本示例，逻辑与 C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp)（capture tensor 样例）一致。

- **Pattern 0**：`MatMul(a, b)` → `Add(..., c)`，图输入 `0/1/2` 对应 `a/b/c`；在模式图上按序 `capture_tensor`：**MatMul 输出**、**Add 输出**（与 C++ `CaptureTensor` 顺序一致）
- **Pattern 1**：`BatchMatMulV2(a, b)` → `Add(..., c)`，同上三输入拓扑及相同 `capture_tensor` 顺序
- **MeetRequirements**：对匹配到的 **Add** 的两路输入做 **FP32（`DT_FLOAT`）** 校验；不满足时打印 `Only support Add inputs are fp32` 并返回 `False`（与 C++ 行为一致；Python 侧 dtype 解析方式见下文说明）
- **Replacement**：`GEMM(r_a, r_b, r_c, alpha=1, beta=1, transpose_a, transpose_b)`（标量 `1.0` 对齐 C++ `CreateScalar(1)`）；`transpose_a` / `transpose_b` 由匹配到的 MatMul / BatchMatMul 节点属性推导（优先 `transpose_x1` / `transpose_x2`，`BatchMatMulV2` 可回退 `adj_x1` / `adj_x2`）
- 继承 **`PatternFusionPass`**，实现 `patterns()` / `meet_requirements()` / `replacement()`；阶段为 **`BeforeInferShape`**

未启用 `enable_const_value_match()` 等额外 matcher 配置，与 C++ 默认匹配行为一致。

## 与 C++ MeetRequirements 的差异说明

C++ 通过 `GNode::GetInputDesc` 读取 Add 两路输入的 TensorDesc。当前公开 Python **`Node` 接口不提供 `GetInputDesc`**，本样例通过 `get_in_data_nodes_and_port_indexes` 沿 **`Data` 节点的 `data_type` 属性**，以及一层 **`MatMul` / `BatchMatMulV2`** 汇聚 dtype 来近似判断。若两路均能解析出 dtype 且任一路非 `DT_FLOAT`，则拦截；若解析不到 dtype（返回 `None`），**不拦截**（避免误杀），与「总能取到 TensorDesc」的 C++ 场景可能存在细微差别。若图在 Add 前有 Cast 等复杂形态，需以实际图为准或等待 Python 侧补齐等价 API。

## 前置条件

- 已 source CANN 环境（`source ${ASCEND_PATH}/set_env.sh`）
- **run 包编译使用的 Python 版本**与执行本样例的 Python 版本一致
- Python 可导入 ES API（通常来自 run 包）：`MatMul`、`Add`、`GEMM`（`ge.es.math` 或 `ge.es.all`）、`BatchMatMulV2`（`ge.es.nn` 或 `ge.es.all`）
- 可导入 GE Python 包（含 `ge.passes` 与 pass 加载链路）

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。

## 使用方式

1. 通过环境变量让 GE 在编译期加载该 Python pass（在 `2_fuse_matmul_add_pass_with_capture_tensor/python` 目录下时）：

```bash
export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_fuse_matmul_add_capture_tensor_pass.py
```

2. 复用 [C++ pass 样例 README](../cpp/README.md) 中的程序编译+程序执行进行验证。


## 预期现象
与 C++ capture tensor 样例类似，在 pattern / MeetRequirements / replacement 被调用时可看到：
```text
Define pattern for FuseMatMulAndAddPass in capture tensor sample
Define MeetRequirements for FuseMatMulAndAddPass in capture tensor sample
Define replacement for FuseMatMulAndAddPass in capture tensor sample
```
使用 `data/torch_forward_2.py`（非 FP32 输入等场景）时，在 `MeetRequirements` 阶段可看到：
```text
Only support Add inputs are fp32
```
（`Define replacement` 仅在通过 `MeetRequirements` 且生成替换图时出现。）


直接运行 `python src/python_fuse_matmul_add_capture_tensor_pass.py` 只会打印注册提示，**不会**触发 GE 编译流程中的 pass 执行。
