# AddZeroPass Python Sample Usage Guide

## Feature Description

This directory provides two pure Python `PatternFusionPass` samples:

- [src/python_add_zero_pass_const_value_match.py](./src/python_add_zero_pass_const_value_match.py)
- [src/python_add_zero_pass.py](./src/python_add_zero_pass.py)

The two samples have different purposes:

- `python_add_zero_pass_const_value_match.py`
  - Demonstrates Python V1 `PatternMatcherConfigBuilder.enable_const_value_match()`
  - Front-loads `Add(x, 0.0f)` to matcher phase via strict const-value-match
  - `@pattern` method uses Python expression to describe strictly matched `Add(x, 0.0f)`, e.g., `return inputs[0] + 0.0`
  - Current `ConstantMatcher::IsMatch` value matching is strict binary match without floating-point tolerance or cross-dtype normalization
  - Better suited as matcher_config example than fully equivalent version of C++ sample

- `python_add_zero_pass.py`
  - Main logic aligned with [C++ pass sample](../cpp/README.md)
  - `@pattern` method uses Python expression to describe `Data + Const` topology, e.g., `return inputs[0] + 0`
  - Multi-input pattern can explicitly declare input count via `x, y, z = inputs[:3]`
  - Multi-pattern pass can declare multiple `@pattern` methods; legacy `patterns(self)` returning multiple `Pattern` / `Graph` still compatible
  - `@pattern` method cannot be used simultaneously with `patterns(self)` to avoid ambiguous pattern declaration sources
  - Python pass framework automatically creates `GraphBuilder`, sets graph outputs, and auto-captures in "visited inputs, `return` outputs" order; legacy explicit `GraphBuilder` syntax still compatible
  - `meet_requirements()` explicitly reads matched `Const.value` and judges zero value per same rules as C++ sample
  - Currently supports `DT_FLOAT`, `DT_DOUBLE`, `DT_INT32` consistent with C++ sample

Recommended expression pattern syntax:

```python
from ge.passes import PatternFusionPass, pattern


class PythonAddZeroPass(PatternFusionPass):
    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0

    def replacement(self, inputs):
        return inputs[0]
```

## Directory Structure

```tree
python/
├── README.md                     // Python sample documentation
├── CMakeLists.txt                // Build script for generating es_all Python ES API
├── src
│   ├── python_add_zero_pass.py   // Python pass implementation file
│   ├── python_add_zero_pass_const_value_match.py // const value match example
```

## Prerequisites

- CANN environment variables configured via `source ${ASCEND_PATH}/set_env.sh`, see [C++ sample README](../cpp/README.md) environment configuration steps
- CANN software package installation per [Environment preparation](../../../../../docs/zh/build.md#1-环境准备)
- Graph compilation flow Python dependencies installed: `attrs`, `decorator`, `sympy`, `numpy`, `psutil`, `scipy`

run package includes GE Python runtime `ge_py` wheel, no need to install separate `ge_py-*.whl`.
Python pass runtime loads `pybind11`-based precompiled binary components. CANN package provides artifacts matching current Python version; if no match, fallback compilation occurs automatically. Fallback compilation requires `pybind11` installed in current Python environment.

## Usage

1. Set Python pass plugin path:

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_add_zero_pass_const_value_match.py
   ```

   For version aligned with C++ main logic:

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_add_zero_pass.py
   ```

2. Follow [C++ sample README](../cpp/README.md) program execution section for verification.
   For offline scenarios, replace `atc` command in C++ README with `pyatc`; both have identical command-line parameters, `pyatc` runs in current Python interpreter process.

3. Notes:

   - These two samples are not standalone execution scripts, directly running `python src/python_add_zero_pass.py` or `python src/python_add_zero_pass_const_value_match.py` won't trigger pass execution
   - Expected output prints after GE compilation flow actually loads this Python pass

## Expected Logs

After successful execution, logs show similar output (actual input_0 name may differ):

```text
[PythonAddZeroConstValueMatchPass] matched=PythonAddZeroConstValueMatchPass_add_zero_pattern captured=input_0:0
[PythonAddZeroPass] matched=PythonAddZeroPass_add_zero_pattern captured=input_0:0 const_dtype=DT_FLOAT zero=True
```
