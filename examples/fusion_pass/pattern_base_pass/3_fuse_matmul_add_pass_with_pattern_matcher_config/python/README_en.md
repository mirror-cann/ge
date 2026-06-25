# FuseMatMulAndAddPass (pattern matcher config) Python Sample Usage Guide

## Feature Description

This directory provides a **pure Python** version of `pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config`, with logic consistent with C++ [`FuseMatMulAndAddPass`](../cpp/src/fuse_matmul_add_pass.cpp).

- Enabled in constructor:
  - `PatternMatcherConfigBuilder().enable_const_value_match()`
  - `PatternMatcherConfigBuilder().enable_ir_attr_match()`
- `@pattern` method defines `MatMul(x, y, transpose_x1=False, transpose_x2=False) + Add(Const)` topology, and creates a shape=[2,2] Const tensor.
- Expression `replacement(self, inputs)` defines `GEMM(x, y, Const, alpha=1, beta=1)`, and creates Const and scalar tensors.
- Registration phase is `PassStage.BEFORE_INFER_SHAPE`.

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
If execution reports missing `GEMM` etc. ES API, follow "ES API Missing Handling (Optional)" below to generate and load `es_all`.

## Usage

Commands below default to execution in `3_fuse_matmul_add_pass_with_pattern_matcher_config/python` directory.

1. Set environment variable for GE to load this Python pass during compilation:

    ```bash
    export ASCEND_GE_PY_PASS_PATH=$PWD/src/python_fuse_matmul_add_pass.py
    ```

2. Follow [C++ sample README](../cpp/README.md) program execution section for verification.

3. Notes:

   - This sample is not a standalone execution script, directly running `python src/python_fuse_matmul_add_pass.py` won't trigger pass execution
   - Expected output prints after GE compilation flow actually loads this Python pass
   - If execution reports missing `GEMM` etc. ES API, generate and load `es_all` first per below, then rerun

## ES API Missing Handling (Optional)

If execution reports missing `GEMM` etc. ES API, generate `es_all` via this Python directory's `CMakeLists.txt`:

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

## Expected Logs

When matching and replacement occur, logs show similar output:

```text
Define pattern for MatmulAddFusionPass in matcher config sample
Define replacement for MatmulAddFusionPass in matcher config sample
```

For `es_forward_2.py` and `es_forward_3.py` intentionally constructed non-match scenarios, typically only pattern definition log appears without entering replacement.
