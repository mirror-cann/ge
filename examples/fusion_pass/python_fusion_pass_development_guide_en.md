# Python Fusion Pass Development Guide

This guide is for developers who want to write GE fusion passes in Python. It is recommended to first read the language-independent mechanism description: [Fusion Pattern Pass Mechanism](../../docs/zh/design/features/fusion_pattern_pass.md).

If you already understand the main workflow of "define pattern, match, filter, replacement, reconnect", you can start coding directly according to this guide.

## 1. Why Consider Python First

Python pass and C++ pass use the same GE matching and replacement mechanism, but Python is better suited for rapid development and runtime integration:

- **Easy Integration**: Configure `.py` files or directories to `ASCEND_GE_PY_PASS_PATH`, GE will load at runtime during compilation phase, without compiling pass into `.so`.
- **Shorter Expression**: `@pattern` can use Python expressions to describe patterns, e.g., `return inputs[0] + 0`.
- **Intuitive Replacement**: Simple replacements can directly write `return inputs[0]`, without manually creating replacement graph.
- **Easy Iteration**: Modify Python file and re-trigger compilation to validate, suitable for iterating rules first.

## 2. Minimal Example: Delete Add(x, 0)

Goal: Replace `Add(x, 0)` in the graph with `x`.

```text
x ----\
        Add ---- out      ==>      x ---- out
 0 ----/
```

Python implementation:

```python
from math import fabs

from ge.graph.types import DataType
from ge.passes import PassStage, PatternFusionPass, pattern, register_fusion_pass


def _scalar_value(value):
    while isinstance(value, list):
        if len(value) != 1:
            return None
        value = value[0]
    return value


def _is_zero(tensor):
    value = _scalar_value(tensor.data)
    if value is None:
        return False
    if tensor.data_type == DataType.DT_FLOAT:
        return fabs(float(value)) < 1e-6
    if tensor.data_type == DataType.DT_DOUBLE:
        return fabs(float(value)) < 1e-15
    if tensor.data_type == DataType.DT_INT32:
        return int(value) == 0
    return False


@register_fusion_pass(name="PythonAddZeroPass", stage=PassStage.BEFORE_INFER_SHAPE)
class PythonAddZeroPass(PatternFusionPass):
    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0

    def meet_requirements(self, match_result):
        for node in match_result.get_matched_nodes():
            if node.type != "Const":
                continue
            return _is_zero(node.get_attr("value"))
        return False

    def replacement(self, inputs):
        return inputs[0]
```

This code does three things:

1. `@pattern` method describes the structure to find: the 0th external input plus a constant.
2. `meet_requirements` checks if the matched constant is really 0.
3. `replacement` returns the 0th external input, equivalent to deleting the matched `Add`.

A complete runnable example is available at [AddZeroPass Python Example](pattern_base_pass/4_add_zero_pass/python/README.md).

## 3. Steps to Write a PatternFusionPass

### 3.1 Import Interfaces

Common imports:

```python
from ge.passes import (
    PassStage,
    PatternFusionPass,
    pattern,
    register_fusion_pass,
)
```

If writing a `DecomposePass`, also need:

```python
from ge.passes import DecomposePass, register_decompose_pass
```

Complete interface documentation is at [Python Passes API](../../docs/zh/user-guides/ge_python/api/Passes.md).

### 3.2 Register Pass

Use `@register_fusion_pass` to register the class to GE:

```python
@register_fusion_pass(name="MyPass", stage=PassStage.BEFORE_INFER_SHAPE)
class MyPass(PatternFusionPass):
    ...
```

`name` must be unique. `stage` indicates execution stage. For initial development, use `PassStage.BEFORE_INFER_SHAPE`, because replacement can still go through GE's subsequent unified shape inference process.

### 3.3 Use @pattern to Define Structure to Match

`@pattern` method receives an `inputs` object. It represents the external input set of the pattern.

```python
@pattern
def add_zero(self, inputs):
    return inputs[0] + 0
```

Here `inputs[0]` is the 0th external input placeholder, not a fixed real node. When matching, GE will map the real tensor connected to this structure to it.

Multi-input scenarios can be written like this:

```python
@pattern
def matmul_add(self, inputs):
    a, b, c = inputs[:3]
    return MatMul(a, b) + c
```

Notes:

- `inputs[i]` will create the `i`th input as needed.
- `inputs[:N]` is used to explicitly declare multiple consecutive inputs.
- `@pattern` will automatically capture visited external inputs and returned pattern outputs. Capture order is fixed: first capture external inputs by input index, then capture pattern outputs by `return` structure order. In the above example, `a/b/c` will be the 0th/1st/2nd captured tensor, and `MatMul(a, b) + c` output will be the 3rd captured tensor in `match_result`.
- Do not directly iterate over `inputs`, because input count is not predetermined.
- One `@pattern` method represents one pattern.
- Multiple topologies need multiple `@pattern` methods.
- `@pattern` cannot be used together with `patterns(self)`.

### 3.4 Use meet_requirements for Condition Filtering (Optional)

If topology matching needs additional checks for dtype, shape, attributes, or constant values, implement `meet_requirements`:

```python
def meet_requirements(self, match_result):
    for node in match_result.get_matched_nodes():
        if node.type == "Const":
            return _is_zero(node.get_attr("value"))
    return False
```

`match_result` is the result of this match. It can get matched real nodes and captured tensors from the pattern. When using `@pattern`, visited external inputs are automatically captured by input index, and `return` pattern outputs are captured by return order; intermediate tensors not used as `return` outputs are not automatically captured.

If only topology matching is sufficient, this method can be omitted; it returns `True` by default.

### 3.5 Use replacement to Define Replacement Structure

The simplest replacement can directly return an input:

```python
def replacement(self, inputs):
    return inputs[0]
```

Can also use expressions to create new structures:

```python
def replacement(self, inputs):
    a, b, c = inputs[:3]
    return GEMM(a, b, c, 1.0, 1.0)
```

If replacement needs to read matched node attributes, add a `match_result` parameter:

```python
def replacement(self, inputs, match_result):
    a, b, c = inputs[:3]
    transpose_a = False
    transpose_b = False
    for node in match_result.get_matched_nodes():
        if node.type not in ("MatMul", "BatchMatMulV2"):
            continue
        try:
            transpose_a = bool(node.get_attr("transpose_x1"))
            transpose_b = bool(node.get_attr("transpose_x2"))
        except RuntimeError:
            pass
        break
    return GEMM(a, b, c, 1.0, 1.0, transpose_a, transpose_b)
```

## 4. When Not to Use @pattern

`@pattern` fits most common topologies, but has a clear boundary: it automatically captures visited external inputs and `return` pattern outputs, but does not automatically capture intermediate tensors not returned as outputs.

If `meet_requirements` or `replacement` needs to read intermediate tensors not returned as `return` outputs, e.g., `MatMul` output, do not use `@pattern`. Instead, explicitly create pattern graph and call `Pattern.capture_tensor` to mark intermediate tensors to read. If only need to read the final output returned by `return`, e.g., `Add` output, continue using `@pattern`.

This approach is closer to C++:

```python
from ge.es.graph_builder import GraphBuilder
from ge.passes import create_pattern, create_replacement


def patterns(self):
    builder = GraphBuilder("pattern")
    a, b, c = builder.create_inputs(3)
    matmul = MatMul(a, b)
    add = matmul + c
    pat = create_pattern(builder.build_and_reset([add]))
    pat.capture_tensor(matmul)
    pat.capture_tensor(add)
    return [pat]


def replacement(self, match_result):
    builder = GraphBuilder("replacement")
    a, b, c = builder.create_inputs(3)
    gemm = GEMM(a, b, c, builder.create_scalar_float(1.0), builder.create_scalar_float(1.0))
    return create_replacement(builder.build_and_reset([gemm]))
```

If only expressing patterns like `Add(x, 0)`, `MatMul + Add`, prefer `@pattern`, code is shorter and closer to optimization logic.

## 5. Capture Tensor

Capture tensor allows retrieving the corresponding real tensor from `match_result` by capture order after pattern matching.

Common uses:

- Check dtype or shape of an output tensor.
- Read original node attributes to pass to new nodes in replacement.
- Print matched location to confirm pass hits expected nodes.

Refer to [capture tensor Python example](pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/python/README.md).

## 6. More Strict Matching: PatternMatcherConfig

Default matcher mainly checks topology and operator types. If wanting to check Const values during matching phase, pass configuration in constructor:

```python
from ge.passes import PatternMatcherConfigBuilder


class PythonAddZeroConstValueMatchPass(PatternFusionPass):
    def __init__(self):
        super().__init__(
            PatternMatcherConfigBuilder()
            .enable_const_value_match()
            .build()
        )

    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0.0

    def replacement(self, inputs):
        return inputs[0]
```

This is shorter, but Const value matching is strict, without floating-point tolerance or cross-dtype normalization. If judgment needs tolerance or more complex logic, put it in `meet_requirements` for reliability.

Refer to [PatternMatcherConfig Python example](pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md).

## 7. Writing DecomposePass

If the goal is "when seeing a certain single operator, decompose it into a set of operators", use `DecomposePass`.

Skeleton is as follows:

```python
from ge.passes import DecomposePass, PassStage, register_decompose_pass


@register_decompose_pass(
    name="PythonMyDecomposePass",
    stage=PassStage.AFTER_INFER_SHAPE,
    op_types=["Conv2D"],
)
class PythonMyDecomposePass(DecomposePass):
    def meet_requirements(self, node):
        return node.get_attr("groups") != 1

    def replacement(self, node):
        # Return replacement graph composed of basic operators
        ...
```

`op_types` determines which types of nodes GE will pass to this pass. `meet_requirements` then determines which of these nodes really need replacement.

Complete example see [DecomposePass Python example](pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/python/README.md).

## 8. Running Python pass

### 8.1 Setting Environment

First set CANN environment variables:

```bash
source ${ASCEND_PATH}/set_env.sh
```

`ASCEND_PATH` points to CANN Toolkit installation directory, more installation path information see [Quick Install](../../docs/zh/quick_install.md).
Python pass runtime will load precompiled binary components built based on `pybind11`, which is related to Python version. CANN package contains precompiled artifacts for multiple Python versions, and defaults to installing artifacts corresponding to current Python version. Runtime will prioritize loading artifacts matching current Python version; if no matching artifacts exist, will enter fallback compilation process, fallback compilation depends on `pybind11` already installed in current Python environment.

Then tell GE where to load Python pass from:

```bash
export ASCEND_GE_PY_PASS_PATH=/path/to/my_pass.py
```

Can also point to directory:

```bash
export ASCEND_GE_PY_PASS_PATH=/path/to/pass_dir/
```

Multiple paths separated by colon:

```bash
export ASCEND_GE_PY_PASS_PATH=/path/to/a.py:/path/to/pass_dir/
```

Detailed scanning rules see [ASCEND_GE_PY_PASS_PATH](../../docs/zh/user-guides/ge_python/env/ASCEND_GE_PY_PASS_PATH.md).

### 8.2 Offline Compilation

Offline scenario suggests using `pyatc` to trigger compilation. `pyatc` and `atc` command line parameters are consistent, but will run in current Python interpreter process, convenient for loading Python pass.

```bash
pyatc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model
```

### 8.3 Online Scenario

In online scenario, set `ASCEND_GE_PY_PASS_PATH` before triggering GE compilation. Examples usually trigger online compilation and execution through `torch_forward.py`.

## 9. Verification and Troubleshooting

Recommend enabling graph dump for every development:

```bash
export DUMP_GE_GRAPH=1
```

Then compare `.pbtxt` before and after replacement:

- `PreRunBegin`: Before pass execution.
- `RunCustomPass...`: After custom pass execution.

If not matched, troubleshoot in this order:

| Phenomenon | Possible Cause | Check Method |
|------|----------|----------|
| Python file not loaded | `ASCEND_GE_PY_PASS_PATH` not set, path does not exist, suffix is not `.py` | First confirm environment variable and path |
| Class loaded but pass not executed | No registration decorator used, or registration stage incorrect | Check `@register_fusion_pass` / `@register_decompose_pass` |
| Pattern not matched | Operator type, input count or output boundary inconsistent | Compare real topology in dump graph |
| Matched but not replaced | `meet_requirements` returned `False` | Print matched node attributes |
| Graph abnormal after replacement | replacement output did not cover Tensor needed by external consumers | Go back to mechanism document to check boundary rules |

When more logs needed, can set:

```bash
export ASCEND_SLOG_PRINT_TO_STDOUT=1
export ASCEND_GLOBAL_LOG_LEVEL=0
```

When using `pyatc`, can also add `--log=debug`.

## 10. Recommended Reading Order

1. [Fusion Pattern Pass Mechanism](../../docs/zh/design/features/fusion_pattern_pass.md)
2. [AddZeroPass Python example](pattern_base_pass/4_add_zero_pass/python/README.md)
3. [MatMul+Add Python example](pattern_base_pass/1_fuse_matmul_add_pass/python/README.md)
4. [capture tensor Python example](pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/python/README.md)
5. [PatternMatcherConfig Python example](pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md)
6. [DecomposePass Python example](pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/python/README.md)
