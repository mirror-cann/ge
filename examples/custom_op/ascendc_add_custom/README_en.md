# Ascend C Operator Graph Integration Sample via TorchAir

## Sample Overview

- Graph construction entry: `PyTorch + TorchAir`
- Operator programming language: `Ascend C`
- Compilation method: `Pre-compiled/Direct invocation`
- Model sinking capability: `Not applicable`
- Core pipeline: `Ascend C kernel -> GE deliverables -> TorchAir graph integration -> Graph mode execution`
- Difference from other samples: This sample focuses on `PyTorch + TorchAir` scenario, does not involve `ATC offline compilation -> om offline model` model sinking, nor Triton kernel.

This sample demonstrates how to register Ascend C custom operators and integrate them into GE graph via `TorchAir`. Using Add operator as an example, the deliverable side completes GE graph integration execution via `EagerExecuteOp`, while the Python side covers both eager invocation and graph mode compiled invocation.

## Applicable Scenarios

- Want to see the minimum graph integration pipeline for Ascend C custom operators in `PyTorch + TorchAir` scenario.
- Want to reference the collaboration between `TORCH_LIBRARY` registration, custom converter and GE custom operator.
- Want to verify numerical results of Ascend C kernel in eager and graph modes.

## Prerequisites

### CANN

- Follow [installation guide](../../../docs/en/quick_install.md#1-environment-preparation) to complete toolkit and ops package installation.

### Framework and Plugins

- Already installed `PyTorch`, `torch_npu` and `TorchAir`.
- `TorchAir` dependent Python version needs to meet its release requirements.

Reference:

- [Ascend Extension for PyTorch](https://gitcode.com/Ascend/pytorch)
- [Ascend Extension for PyTorch Ascend Community Documentation](https://hiascend.com/document/redirect/Pytorch-index)

### Environment Variables

- `ASCEND_HOME_PATH`
- `LD_LIBRARY_PATH` and other CANN runtime related variables

### Additional Dependencies

```bash
pip3 install expecttest
```

## Quick Run

Execute in `examples/custom_op/ascendc_add_custom` directory:

### Recommended Method

```bash
mkdir -p build
cd build
cmake ..
make -j"$(nproc)"
export ASCEND_CUSTOM_OPP_PATH="$(pwd):$ASCEND_CUSTOM_OPP_PATH"
python3 ../script/add_custom_test.py
```

If the terminal shows `Ran 1 test` and `OK`, the sample runs successfully.

### Step-by-step Method

1. First execute `cmake ..` and `make -j"$(nproc)"` to generate `build/libcust_opapi.so`.
2. Then execute `export ASCEND_CUSTOM_OPP_PATH="$(pwd):$ASCEND_CUSTOM_OPP_PATH"` to add the directory where `libcust_opapi.so` is located to environment variable.
3. Finally execute `python3 ../script/add_custom_test.py` to run eager and TorchAir graph mode tests.
4. If you need to execute installation target, you can additionally run `cmake --install build`.

## Directory Structure and Key Files

```text
ascendc_add_custom
├── CMakeLists.txt
├── README.md
├── add_custom_kernel
│   ├── add_custom.asc        // Ascend C kernel implementation and PyTorch registration code
│   ├── add_custom_kernel.h   // kernel declaration
│   └── custom_op.cpp         // GE deliverables, implementing Execute graph integration logic
└── script
    └── add_custom_test.py    // Python test script, covering eager and graph modes
```

Key files:

- `add_custom_kernel/add_custom.asc`
  Ascend C Add kernel implementation file, also contains PyTorch custom operator registration logic.
- `add_custom_kernel/custom_op.cpp`
  Custom operator GE deliverables, implementing `Execute` path and completing GE graph integration.
- `script/add_custom_test.py`
  Python test script, covering eager execution and TorchAir graph mode compiled execution.
- `CMakeLists.txt`
  Compiles `libcust_opapi.so` and configures PyTorch, TorchAir, CANN dependencies.

## Core Pipeline

1. Implement Ascend C Add kernel in `add_custom_kernel/add_custom.asc` and complete PyTorch custom operator registration.
2. Implement `EagerExecuteOp` in `add_custom_kernel/custom_op.cpp` to register custom operator to GE.
3. Load `libcust_opapi.so` via `script/add_custom_test.py`, execute and verify results in eager and TorchAir graph modes.

## Build Artifacts

- `build/libcust_opapi.so`
  Custom operator deliverables used by GE/TorchAir.

## Result Verification

When successful, you can observe:

- Terminal output contains `Ran 1 test`.
- Terminal output contains `OK`.
- No `Numerical values do not match within tolerance` error during testing.

If failed, priority checks:

- Whether `ASCEND_HOME_PATH`, `LD_LIBRARY_PATH` and other environment variables are correct.
- Whether `torch`, `torch_npu`, `TorchAir` versions match.
- Whether `build/libcust_opapi.so` has been successfully generated and can be loaded by Python script.
- Whether current environment has available NPU, script will directly verify `torch.npu.is_available()`.

## Notes / Limitations

- Supported products: `Atlas A2 training series products / Atlas A2 inference series products`.
- Sample currently uses `float16` `8 x 2048` input for result verification.
- `script/add_custom_test.py` depends on available NPU environment, cannot pass in CPU environment.
- This sample demonstrates `PyTorch + TorchAir` graph integration path, does not involve model sinking to om offline model.

## Appendix

### Operator Specification

| Item | Content |
| --- | --- |
| Operator type | `AddCustom` |
| Input | `x`, `y` |
| Output | `z` |
| Input shape | `8 x 2048` |
| Output shape | `8 x 2048` |
| Data type | `float16` |
| Format | `ND` |
| Kernel name | `add_custom` |

### Operator Implementation Supplement

Ascend C Add kernel execution process can be summarized in three steps:

1. `CopyIn`: Move input from Global Memory to on-chip Local Memory.
2. `Compute`: Perform addition operation on two LocalTensors.
3. `CopyOut`: Move computation result back to Global Memory corresponding to output Tensor.

### Registration Mechanism Supplement

- `TORCH_LIBRARY_FRAGMENT` is used to define Python-side visible operator signatures.
- `TORCH_LIBRARY_IMPL` is used to bind operator implementation to `PrivateUse1`, `XLA`, `Meta` and other `DispatchKey`s.
- `@torchair.register_fx_node_ge_converter(...)` in `script/add_custom_test.py` is responsible for mapping PyTorch nodes to GE custom operator `AddCustom`.
