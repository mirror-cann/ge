# FuseMatMulAndAddPass Python Sample Usage Guide

This directory provides a **pure Python** version of `pattern_base_pass/1_fuse_matmul_add_pass`, with logic consistent with C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp):

- **Pattern 0**: `MatMul(a, b)` → `Add(..., c)`, graph inputs `0/1/2` correspond to `a/b/c`
- **Pattern 1**: `BatchMatMulV2(a, b)` → `Add(..., c)`, same three-input topology
- **Replacement**: `GEMM(r_a, r_b, r_c, alpha=1, beta=1)` (scalar `1.0` aligns with C++ `CreateScalar(1)`)
- Inherits **`PatternFusionPass`**, uses `@pattern` method to declare patterns, expression `replacement(self, inputs)` to declare replacement graph; phase is **`BeforeInferShape`**

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

run package typically includes `ge_py` wheel, no need to install separate `ge_py-*.whl`.
Python pass runtime loads `pybind11`-based precompiled binary components. CANN package provides artifacts matching current Python version; if no match, fallback compilation occurs automatically. Fallback compilation requires `pybind11` installed in current Python environment.
If execution reports missing `BatchMatMulV2`, `GEMM` etc. ES API, follow "ES API Missing Handling (Optional)" below to generate and load `es_all`.

## Usage

Commands below default to execution in `1_fuse_matmul_add_pass/python` directory.

1. Set environment variable for GE to load this Python pass during compilation:

```bash
export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_fuse_matmul_add_pass.py
```

1. Follow [C++ sample README](../cpp/README.md) program execution section for verification.

2. Notes:

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

Similar to C++ sample, when pattern/replacement invoked:

```text
Define pattern for FuseMatMulAndAddPass
Define replacement for FuseMatMulAndAddPass
```

Note: Since `@pattern` method is used, the pattern graph names in logs are `PythonFuseMatMulAndAddPass_matmul_add_pattern` and `PythonFuseMatMulAndAddPass_batch_matmul_add_pattern`.
