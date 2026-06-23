# MoveReluBeforeConcatPass Python Example Usage Guide

This directory provides a **pure Python** version example of `graph_base_pass/2_move_relu_before_concat_pass`, with logic identical to the C++ version:

- Scan `ConcatV2 -> Relu` structure
- Build replacement subgraph (move Relu to each input of Concat)
- Use `SubgraphBoundary` + `SubgraphRewriter.replace()` for subgraph replacement

> **FusionBasePass vs PatternFusionPass**
>
> The pass in this example inherits FusionBasePass. Unlike PatternFusionPass, here pass logic is implemented by overriding `run()` function.
> `run()` function input contains reference to graph object. In this example scenario, Concat node input count is not fixed,
> difficult to represent with fixed pattern. In `run()` function can dynamically build Boundary based on matched Concat node in target graph, achieving higher flexibility.

## Directory Structure

```tree
python/
├── README.md                     // Python example description
├── CMakeLists.txt                // Build script for generating es_all Python ES API
├── src
│   ├── python_move_relu_before_concat_pass.py // Python pass implementation file
```

## Prerequisites

- Completed CANN environment variable setup via `source ${ASCEND_PATH}/set_env.sh`. For more guidance, refer to [C++ Example README](../cpp/README.md) environment variable configuration step
- Python environment can import ES API (usually from run package/ops package):
  - `ge.es.math.ConcatV2`
  - `ge.es.nn.Relu` (if this symbol is unavailable, confirm ES Python API installation is complete)
- Can import GE Python package (contains `ge.graph`, `ge.passes` and pass loading chain)

Python pass runtime loads precompiled binary components based on `pybind11`. CANN package prioritizes providing artifacts matching current Python version; if no matching artifact, automatically enters fallback compilation flow. Fallback compilation requires `pybind11` installed in current Python environment.
If execution reports missing `ConcatV2`, `Relu` etc. ES API, follow "ES API Missing Handling (Optional)" below to generate and load `es_all`.

## Usage

The following commands are executed in `2_move_relu_before_concat_pass` directory by default.

1. Configure environment variables:

    ```bash
    source ${ASCEND_PATH}/set_env.sh
    export DUMP_GE_GRAPH=1
    export ASCEND_GE_PY_PASS_PATH=$PWD/python/src/python_move_relu_before_concat_pass.py
    ```

2. Generate AIR model:

    ```bash
    cd cpp/data
    python es_gen_air.py
    ```

3. Use `pyatc` offline compilation to trigger Python pass, modify `soc_version` per actual environment:

    ```bash
    pyatc --model=./graph.air --framework=1 --soc_version=xxx --output=./model
    cd ../..
    ```

4. Notes:

   - `pyatc` command line parameters are identical to `atc`, but run in current Python interpreter process
   - If execution reports missing `ConcatV2`, `Relu` etc. ES API, first generate and load `es_all` per instructions below, then rerun

## ES API Missing Handling (Optional)

If execution reports missing `ConcatV2`, `Relu` etc. ES API, can generate `es_all` via `CMakeLists.txt` in this Python directory:

```bash
cd python
cmake -S . -B build
cmake --build build --target build_es_all -j$(nproc)
```

Install generated Python package and let current Python process find the package and corresponding dynamic library:

```bash
pip install --force-reinstall --upgrade --target ./build/whl_package ./build/es_output/whl/es_all-1.0.0-py3-none-any.whl
export PYTHONPATH="$PWD/build/whl_package:${PYTHONPATH:-}"
export LD_LIBRARY_PATH="$PWD/build/es_output/lib64:${LD_LIBRARY_PATH:-}"
cd ..
```

## Expected Result

Similar log output:

```text
PythonMoveReluBeforeConcatPass
Replacement of PythonMoveReluBeforeConcatPass succeeded
```
