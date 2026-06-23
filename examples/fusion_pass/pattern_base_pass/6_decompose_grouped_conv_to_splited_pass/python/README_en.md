# DecomposeGroupedConvToSplitedPass Python Sample Usage Guide

## Function Description

This directory provides a **pure Python** version of `pattern_base_pass/6_decompose_grouped_conv_to_splited_pass`, with logic consistent with the C++ version:

- `op_types=["Conv2D"]`, only matches `Conv2D` nodes in the original graph
- `meet_requirements()` checks `groups != 1` and `data_format == "NCHW"`
- `replacement()` constructs a replacement graph using `Split(input) + Split(filter) + multiple Conv2D + Concat`
- Updates input/output `TensorDesc` for each `Conv2D` in the replacement graph, setting `FORMAT_NCHW` / `DT_FLOAT`
- Executes `InferShape` on the replacement graph to complete shape inference

## Directory Structure

```tree
python/
├── README.md                     // Python sample documentation
├── CMakeLists.txt                // Build script for generating Python ES API
├── src
│   ├── python_decompose_pass.py  // Python pass implementation file
```

## Prerequisites

- CANN environment variables have been set via `source ${ASCEND_PATH}/set_env.sh`. For more details, refer to the environment variable configuration step in [C++ Sample README](../cpp/README.md)
- CANN software package installation, refer to [Environment Preparation](../../../../../docs/zh/build.md#1-环境准备)
- Python dependencies for graph compilation installed: `attrs`, `decorator`, `sympy`, `numpy`, `psutil`, `scipy`

The run package already includes the `ge_py` wheel required for GE Python runtime. There is no need to separately install `ge_py-*.whl` in this section.
Python pass runtime loads pre-compiled binary components based on `pybind11`. CANN package prioritizes products matching the current Python version; if no matching product exists, it automatically enters the fallback compilation process. Fallback compilation requires `pybind11` to be installed in the current Python environment.
If execution prompts missing ES APIs such as `Conv2D`, follow the "Handling Missing ES API (Optional)" section below to generate and load `es_all`.

## Usage

The following commands are executed in the `6_decompose_grouped_conv_to_splited_pass/python` directory by default.

1. Set the Python pass plugin path:

   ```bash
   export ASCEND_GE_PY_PASS_PATH="$PWD/src/python_decompose_pass.py"
   ```

2. Refer to the program execution section in [C++ Sample README](../cpp/README.md) for verification.
   For offline scenarios, replace the `atc` command in the C++ README with `pyatc`; both have identical command-line parameters, and `pyatc` runs within the current Python interpreter process.

3. Notes:

   - This sample is not a standalone execution script; running `python src/python_decompose_pass.py` directly will not trigger pass execution
   - Expected output will be printed after GE compilation process actually loads this Python pass
   - If execution reports missing ES APIs such as `Conv2D`, first generate and load `es_all` as described below, then rerun

## Handling Missing ES API (Optional)

If execution reports missing ES APIs such as `Conv2D`, you can generate `es_all` in the `6_decompose_grouped_conv_to_splited_pass/python` directory:

```bash
cmake -S . -B build
cmake --build build --target build_es_all -j$(nproc)
```

Install the generated Python package and make it accessible to the current Python process along with its dynamic libraries:

```bash
pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_all-1.0.0-py3-none-any.whl
export PYTHONPATH="$PWD/build/whl_package:${PYTHONPATH:-}"
export LD_LIBRARY_PATH="$PWD/build/es_output/lib64:${LD_LIBRARY_PATH:-}"
```

## Expected Logs

After successful execution, similar output will appear in the logs:

```text
Define MeetRequirements for PythonDecomposeGroupedConvToSplitedPass
Define Replacement for PythonDecomposeGroupedConvToSplitedPass
```
