# FuseMatMulAndAddPass（capture tensor）Python 样例使用指导

本目录提供 `pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor` 的 **纯 Python** 版本示例，逻辑与 C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp)（capture tensor 样例）一致。

- **Pattern 0**：`MatMul(a, b)` → `Add(..., c)`，图输入 `0/1/2` 对应 `a/b/c`；在模式图上按序调用 `Pattern.capture_tensor`：**MatMul 输出**、**Add 输出**（与 C++ `CaptureTensor` 顺序一致）
- **Pattern 1**：`BatchMatMulV2(a, b)` → `Add(..., c)`，同上三输入拓扑及相同 `Pattern.capture_tensor` 顺序
- **MeetRequirements**：对匹配到的 **Add** 的两路输入通过 `get_input_desc(index)` 读取 TensorDesc 并做 **FP32（`DT_FLOAT`）** 校验；不满足时打印 `Only support Add inputs are fp32` 并返回 `False`（与 C++ 行为一致）
- **Replacement**：`GEMM(r_a, r_b, r_c, alpha=1, beta=1, transpose_a, transpose_b)`（标量 `1.0` 对齐 C++ `CreateScalar(1)`）；`transpose_a` / `transpose_b` 由匹配到的 MatMul / BatchMatMul 节点属性推导（优先 `transpose_x1` / `transpose_x2`，`BatchMatMulV2` 可回退 `adj_x1` / `adj_x2`）
- 继承 **`PatternFusionPass`**，实现 `patterns()` / `meet_requirements()` / `replacement()`；阶段为 **`BeforeInferShape`**

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
- **run 包编译使用的 Python 版本**与执行本样例的 Python 版本一致。原因是 Python pass 相关扩展模块等与编译时 Python ABI 相关，版本不一致时可能导入失败或加载异常
- 可导入 GE Python 包（含 `ge.passes` 与 pass 加载链路）

run 包已包含 GE Python 运行时所需的 `ge_py` wheel，本节不需要再单独安装 `ge_py-*.whl`。
如果执行时提示缺少 `BatchMatMulV2`、`GEMM` 等 ES API，再按下文“ES API 缺失时处理（可选）”生成并加载 `es_all`。

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
- 再按 [C++ 样例 README](../cpp/README.md) 的配置环境变量步骤，完成环境变量设置
- 最后按本文使用方式设置 `ASCEND_GE_PY_PASS_PATH` 并参考 C++ 样例的运行步骤

## 使用方式

以下命令默认在 `2_fuse_matmul_add_pass_with_capture_tensor/python` 目录执行。

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
与 C++ capture tensor 样例类似，在 pattern / MeetRequirements / replacement 被调用时可看到：
```text
Define pattern for FuseMatMulAndAddPass in capture tensor sample
Define MeetRequirements for FuseMatMulAndAddPass in capture tensor sample
Define replacement for FuseMatMulAndAddPass in capture tensor sample
```
使用 `data/torch_forward_2.py`（非 FP32 输入等场景）时，在 `MeetRequirements` 阶段可看到：
```text
Only support Add inputs are DT_FLOAT, got input0=DT_FLOAT16, input1=DT_FLOAT16
```
（`Define replacement` 仅在通过 `MeetRequirements` 且生成替换图时出现。）
