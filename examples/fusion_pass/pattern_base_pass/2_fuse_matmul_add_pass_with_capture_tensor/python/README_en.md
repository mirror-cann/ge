# FuseMatMulAndAddPass (capture tensor) Python Sample Usage Guide

This directory provides a **pure Python** version of `pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor`, with logic consistent with C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp) (capture tensor sample).

- **Pattern 0**: `MatMul(a, b)` → `Add(..., c)`, graph inputs `0/1/2` correspond to `a/b/c`; call `Pattern.capture_tensor` sequentially on pattern graph: **MatMul output**, **Add output** (consistent with C++ `CaptureTensor` order)
- **Pattern 1**: `BatchMatMulV2(a, b)` → `Add(..., c)`, same three-input topology and same `Pattern.capture_tensor` order
- **MeetRequirements**: Read TensorDesc via `get_input_desc(index)` for both inputs of matched **Add** and validate **FP32 (`DT_FLOAT`)**; print `Only support Add inputs are fp32` and return `False` when not satisfied (consistent with C++ behavior)
- **Replacement**: `GEMM(r_a, r_b, r_c, alpha=1, beta=1, transpose_a, transpose_b)` (scalar `1.0` aligns with C++ `CreateScalar(1)`); `transpose_a` / `transpose_b` derived from matched MatMul / BatchMatMul node attributes (prefer `transpose_x1` / `transpose_x2`, `BatchMatMulV2` can fallback to `adj_x1` / `adj_x2`)
- Inherits **`PatternFusionPass`**, implements `patterns()` / `meet_requirements()` / `replacement()`; phase is **`BeforeInferShape`**

No additional matcher configuration like `enable_const_value_match()` enabled, consistent with C++ default matching behavior.

## Directory Structure

```tree
python/
├── README.md                     // Python sample documentation
├── CMakeLists.txt                // Build script for generating es_all Python ES API
├── src
│   ├── python_fuse_matmul_add_pass.py // Python pass implementation file
```

## Prerequisites

- CANN environment variables configured via `source ${ASCEND_PATH}/set_env.sh`, see [C++ sample README](../cpp/README.md) environment configuration steps
- GE Python package importable (includes `ge.passes` and pass loading chain)

run package includes GE Python runtime `ge_py` wheel, no need to install separate `ge_py-*.whl`.
Python pass runtime loads `pybind11`-based precompiled binary components. CANN package provides artifacts matching current Python version; if no match, fallback compilation occurs automatically. Fallback compilation requires `pybind11` installed in current Python environment.
If execution reports missing `BatchMatMulV2`, `GEMM` etc. ES API, follow "ES API Missing Handling (Optional)" below to generate and load `es_all`.

## Usage

Commands below default to execution in `2_fuse_matmul_add_pass_with_capture_tensor/python` directory.

1. Set environment variable for GE to load this Python pass during compilation:

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_fuse_matmul_add_pass.py
   ```

2. Follow [C++ sample README](../cpp/README.md) program execution section for verification.

3. Notes:

   - This sample is not a standalone execution script, directly running `python src/python_fuse_matmul_add_pass.py` won't trigger pass execution
   - If execution reports missing `BatchMatMulV2`, `GEMM` etc. ES API, generate and load `es_all` first per below, then rerun

## ES API Missing Handling (Optional)

If execution reports missing `BatchMatMulV2`, `GEMM` etc. ES API, generate `es_all` via this Python directory's `CMakeLists.txt`:

```bash
cmake -S . -B build
cmake --build build --target build_es_all -j$(nproc)
```

Install generated Python package and ensure current Python process can find the package and corresponding dynamic library:

```bash
pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_all-1.0.0-py3-none-any.whl
export PYTHONPATH="$PWD/build/whl_package:${PYTHONPATH:-}"
export LD_LIBRARY_PATH="$PWD/build/es_output/lib64:${LD_LIBRARY_PATH:-}"
```

## Expected Behavior

Similar to C++ capture tensor sample, when pattern / MeetRequirements / replacement invoked:

```text
Define pattern for FuseMatMulAndAddPass in capture tensor sample
Define MeetRequirements for FuseMatMulAndAddPass in capture tensor sample
Define replacement for FuseMatMulAndAddPass in capture tensor sample
```

Using `data/torch_forward_2.py` (non-FP32 input scenarios), in `MeetRequirements` phase:

```text
Only support Add inputs are DT_FLOAT, got input0=DT_FLOAT16, input1=DT_FLOAT16
```

(`Define replacement` only appears after passing `MeetRequirements` and generating replacement graph.)
