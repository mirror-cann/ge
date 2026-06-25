# AddCustomZeroPass Python Sample Usage Guide

## Feature Description

This directory provides a **pure Python** version of `pattern_base_pass/5_add_zero_pass_in_custom_op`, with logic consistent with C++ version:

- Uses `@pattern` method to define two patterns:
  - `Data + Const -> Identity -> AddCustom -> AddCustom`
  - `Data + Const -> Identity -> TensorMove -> AddCustom -> AddCustom`
- `meet_requirements()` reads matched `Const.value` and judges zero value per same rules as C++ sample
- Expression `replacement(self, inputs)` replaces matched add-zero structure with `AddCustom(inputs[0], inputs[1])`
- `@pattern` method obtains the framework's `GraphBuilder` via `inputs[0].get_owner_builder()`, uses `create_const_float(0.0)` to explicitly create a DT_FLOAT scalar Const node; the framework automatically sets graph outputs and captures inputs

## Directory Structure

```tree
python/
├── README.md                     // Python sample documentation
├── CMakeLists.txt                // Build script for generating AddCustom / es_all Python ES API
├── src
│   ├── python_addcustom_zero_pass.py // Python pass implementation file
```

## Prerequisites

- CANN environment variables configured via `source ${ASCEND_PATH}/set_env.sh`, see [C++ sample README](../cpp/README.md) environment configuration steps
- CANN software package installation per [Environment preparation](../../../../../docs/zh/build.md#1-环境准备)
- Graph compilation flow Python dependencies installed: `attrs`, `decorator`, `sympy`, `numpy`, `psutil`, `scipy`

run package includes GE Python runtime `ge_py` wheel, no need to install separate `ge_py-*.whl`.
Python pass runtime loads `pybind11`-based precompiled binary components. CANN package provides artifacts matching current Python version; if no match, fallback compilation occurs automatically. Fallback compilation requires `pybind11` installed in current Python environment.
However, run package doesn't include this sample's `AddCustom` Python ES API, need to generate `es_custom` wheel from `cpp/proto/add_custom_proto.cc` before running Python pass.

## ES API Description

- `AddCustom` comes from this sample's generated `es_custom` wheel, generate and load per Usage steps 1, 2 below
- `Identity`, `TensorMove` typically come from built-in ES API in run package; if execution reports missing these interfaces, first confirm CANN environment variables set, if necessary generate and load `es_all` per "Built-in ES API Missing Handling (Optional)" below then rerun

## Preparation

Before writing Python pass for custom operators, need to complete custom operator project creation, operator implementation, operator package compilation deployment and operator adaptation development, refer to [C++ sample README](../cpp/README.md) preparation section.

## Usage

1. Generate `AddCustom` Python ES API:

   ```bash
   cmake -S . -B build
   cmake --build build --target build_es_custom -j$(nproc)
   ```

   This directory's `CMakeLists.txt` reuses `../cpp/proto/add_custom_proto.cc` from C++ sample, generates `es_custom` C/C++ library and Python wheel via `add_es_library_and_whl`.

2. Install generated Python package and ensure current Python process can find the package and corresponding dynamic library:

   ```bash
   pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_custom-1.0.0-py3-none-any.whl
   export PYTHONPATH="$PWD/build/whl_package:${PYTHONPATH:-}"
   export LD_LIBRARY_PATH="$PWD/build/es_output/lib64:${LD_LIBRARY_PATH:-}"
   ```

3. Built-in ES API Missing Handling (Optional):

   If execution reports missing `Identity`, `TensorMove` etc. built-in ES API, after completing above `cmake -S . -B build`, additionally generate and install `es_all`:

   ```bash
   cmake --build build --target build_es_all -j$(nproc)
   pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_all-1.0.0-py3-none-any.whl
   ```

   `PYTHONPATH` and `LD_LIBRARY_PATH` reuse settings from previous step.

4. Set Python pass plugin path:

   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/python_addcustom_zero_pass.py
   ```

5. Follow [C++ sample README](../cpp/README.md) verification section for verification.

6. Notes:

   - This sample is not a standalone execution script, directly running `python src/python_addcustom_zero_pass.py` won't trigger pass execution
   - Expected output prints after GE compilation flow actually loads this Python pass

## Expected Logs

After successful execution, logs show similar output:

```text
Define pattern for PythonAddCustomZeroPass
Define MeetRequirements for PythonAddCustomZeroPass
[PythonAddCustomZeroPass] matched=PythonAddCustomZeroPass_addcustom_zero_pattern const_dtype=DT_FLOAT16 zero=True
Define replacement for PythonAddCustomZeroPass
```
