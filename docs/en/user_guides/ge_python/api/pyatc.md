# pyatc

## Product Support Status

| Product | Support Status |
| :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```bash
# Command line mode
python -m ge.pyatc [ATC parameters...]

# Python code mode
from ge.pyatc import main
```

## Functionality Description

`pyatc` module provides Python entry equivalent to `atc` (Ascend Tensor Compiler) command line tool, facilitating users to execute ATC compilation commands within specified Python interpreter process.

The core use of this module is to solve scenarios where ATC process needs to use specific Python interpreter, such as Python Pass plugin loading and execution. Through `pyatc`, users can ensure ATC uses current Python environment instead of system default Python.

## Function Prototypes

### main

```python
def main(cmdline_argv0=None, args=None) -> None
```

ATC command line main entry function. Encodes parameters and calls underlying C++ ATC main function to execute compilation, uses return value as exit code to exit process.

## Parameter Description

### main

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| cmdline_argv0 | Optional[str] | No | Custom argv[0], i.e., command line program name. If None, uses `sys.argv[0]` (when argv[0] is `__main__.py`, automatically replaced with `sys.executable + " -m ge.pyatc"`) |
| args | Optional[List[str]] | No | ATC command line parameter list. If None, uses `sys.argv[1:]` |

## Return Value Description

`main` function has no return value. After execution completes, calls `sys.exit()` to exit process with underlying ATC return code.

## Constraint Description

- `main` function calls `sys.exit()` to terminate process, not suitable for scenarios where subsequent logic needs to continue executing.
- ATC command line parameters are consistent with `atc` command line tool, specific parameter description please refer to ATC related documentation.
- This module requires CANN environment to be properly installed and configured.

## Usage Example

```bash
# Command line mode, equivalent to atc command (example only, atc command complete validity not guaranteed)
python -m ge.pyatc --model=model.onnx --soc_version=Ascend910B1 --output=model
# Or
pyatc --model=model.onnx --soc_version=Ascend910B1 --output=model


# With Pass path scenario (example only, atc command complete validity not guaranteed)
export ASCEND_GE_PY_PASS_PATH=/path/to/my_pass.py
python -m ge.pyatc --model=model.onnx --soc_version=Ascend910B1 --output=model
# Or
pyatc --model=model.onnx --soc_version=Ascend910B1 --output=model
```
