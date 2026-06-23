# Data Dependent Shape Custom Operator (Type III Operator) Sample

## Sample Overview

- Graph construction entry: `GE`
- Operator programming language: `Ascend C`
- Compilation method: `.asc` and `.cpp` compiled in same target
- Model sinking capability: `Not applicable`
- Core pipeline: `Ascend C kernel and host-side custom op compiled in same library -> GE deliverables -> In-process graph construction -> Session::RunGraph -> hybrid/RT2 dynamic execution`
- Difference from other samples: This sample focuses on shape buffer usage in unknown graph dynamic execution scenarios for type III custom operators.

This sample demonstrates a minimum runnable type III custom operator execution pipeline: after in-process graph construction, directly call `Session::RunGraph`, compile Ascend C kernel and host-side custom op to same `libwhere_like_custom_op.so` by CMake, then `WhereLikeCustom::Execute()` allocates maximum output and shape buffer, finally writes back actual output shape on device side.

`WhereLikeCustom` input is `bool[N]`, used to return indices of elements with value `true` in input, output is `int64[true_count, rank]`. Since `true_count` depends on runtime input data, only output upper bound can be determined at compile time, actual shape needs to be passed back through type III custom op shape buffer protocol after execution.

## Applicable Scenarios

- Want to see minimum implementation of type III custom operators in `Session::RunGraph` dynamic execution pipeline.
- Want to reference how user synchronizes stream, reads back shape buffer and updates output shape in `Execute`.
- Want to see complete process of `.asc` and `.cpp` compiled directly together and initiating kernel calls in `Execute`.
- Not suitable for understanding `ATC offline compilation -> om offline model` sinking pipeline.

## Prerequisites

### CANN

- CANN environment has been correctly installed and configured, e.g., executed `source ${ASCEND_HOME_PATH}/set_env.sh`.
- Current environment has `ACL`, `GE`, `Graph`, `Ascend C` related headers and libraries.
- Follow [installation guide](../../../docs/zh/quick_install.md) to complete toolkit and ops package installation.

### Framework and Plugins

- This sample does not depend on PyTorch, TensorFlow or TorchAir.

### Environment Variables

- `ASCEND_HOME_PATH`
- `ASCEND_CUSTOM_OPP_PATH` will be automatically appended as current sample's `output/` in `run.sh`

### Additional Dependencies

- `cmake`
- `g++`

## Quick Run

Execute in `examples/custom_op/data_dependent_shape_custom` directory:

### Recommended Method

```bash
source ${ASCEND_HOME_PATH}/set_env.sh
bash run.sh
```

`run.sh` will automatically complete configure, build, install and append `output/` to `ASCEND_CUSTOM_OPP_PATH`. If successful, terminal will print:

```text
output shape: [4, 1]
output values: 0 2 4 7
```

### Step-by-step Method

```bash
source ${ASCEND_HOME_PATH}/set_env.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"

cd build
./data_dependent_shape_custom_session_run
cd ..
```

Where `export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"` is used to add custom operator package root directory to environment variable, then GE will load deliverables according to `output/op_graph/lib/<os>/<arch>/libwhere_like_custom_op.so` rule.

## Directory Structure and Key Files

```text
data_dependent_shape_custom
├── CMakeLists.txt
├── README.md
├── run.sh
├── ge
│   ├── custom_op.cpp                    // EagerExecuteOp + ShapeInferOp main process implementation
│   ├── where_like_custom.h       // WhereLikeCustom proto definition
│   └── where_like_custom_kernel.asc // Ascend C kernel source code
└── session_run
    └── main.cc                          // In-process graph construction and direct Session::RunGraph call
```

Key files:

- `ge/custom_op.cpp`
  Custom operator core main process, implements `Execute`, `InferShape` and `InferDataType`; where `Execute` is responsible for allocating maximum output, shape buffer, calling `.asc` exported launch wrapper, and reading back shape buffer to update output shape after kernel completion, `InferShape` / `InferDataType` is responsible for compile-time output shape / dtype derivation.
- `ge/where_like_custom_kernel.asc`
  Device kernel and host-side launch wrapper implementation, responsible for writing output data and shape buffer.
- `session_run/main.cc`
  Build minimum graph and execute directly via `Session::AddGraph + Session::RunGraph`.
- `run.sh`
  Chain configure, build, install and run process.

## Core Pipeline

1. `session_run/main.cc` builds minimum graph containing `Data -> WhereLikeCustom` and sets input/output description to dynamic shape, making the whole graph go `unknown graph` execution pipeline.
2. `InferShape` / `InferDataType` in `ge/custom_op.cpp` gives output shape / dtype at graph construction phase; this sample does not depend on framework lowering to insert shape write-back nodes.
3. `CMakeLists.txt` compiles `ge/custom_op.cpp` and `ge/where_like_custom_kernel.asc` together to `libwhere_like_custom_op.so`.
4. `ge/custom_op.cpp` allocates output via `ctx->MallocOutputTensor(...)` according to maximum shape in `Execute` callback, then allocates shape buffer via first `ctx->MallocWorkSpace(...)`.
5. `ge/where_like_custom_kernel.asc` writes output indices and shape buffer on device side, where shape buffer is used to pass back actual output shape.
6. `Execute` in `ge/custom_op.cpp` synchronizes current stream after launch, copies shape buffer back to host, parses real shape and updates output tensor's logical shape and valid size.
7. After execution, `session_run/main.cc` reads output tensor and prints actual shape and output values.

## Build Artifacts

- `output/op_graph/lib/linux/x86_64/libwhere_like_custom_op.so`
  Custom operator deliverables used by GE in Linux x86_64 environment; aarch64 environment corresponds to `output/op_graph/lib/linux/aarch64/libwhere_like_custom_op.so`.
- `output/op_graph/include/where_like_custom.h`
  Operator proto header file that can be directly used by graph construction side.
- `build/data_dependent_shape_custom_session_run`
  Direct `Session::RunGraph` execution program.

## Result Verification

When successful, you can observe:

- `output/op_graph/lib/<os>/<arch>/libwhere_like_custom_op.so` has been generated.
- `output/op_graph/include/where_like_custom.h` has been generated.
- Terminal output contains `output shape: [4, 1]`.
- Terminal output contains `output values: 0 2 4 7`.

If failed, priority checks:

- Whether `ASCEND_HOME_PATH` has been set and CANN environment has been correctly `source`d.
- Whether `ASCEND_CUSTOM_OPP_PATH` already contains current sample's `output/`.
- Whether `output/op_graph/lib/<os>/<arch>/libwhere_like_custom_op.so` and `output/op_graph/include/where_like_custom.h` have been generated.
- Whether current environment has available NPU and available Ascend C compilation environment.

## Notes / Limitations

- `WhereLikeCustom` current sample input is one-dimensional `bool[8]`, so actual output is matching position indices, shape is `[true_count, 1]`.
- `.asc` compilation parameter currently fixed as `--npu-arch=dav-2201`, if target chip is different need to adjust `CMakeLists.txt`.

## Appendix

### Operator Specification

| Item | Content |
| --- | --- |
| Operator type | `WhereLikeCustom` |
| Input | `x` |
| Output | `y` |
| Input shape | `N` |
| Output shape upper bound | `[N, rank(x)]` |
| Output actual shape | `[true_count, rank(x)]` |
| Input data type | `bool` |
| Output data type | `int64` |
| Format | `ND` |
| Kernel name | `where_like_custom` |

### Shape Buffer Convention

This sample uses user-defined shape buffer protocol and user parses and writes back actual output shape in `Execute`. Current kernel will write:

- `shape[0] = 2U`
- `shape[1] = true_count`
- `shape[2] = rank`

For current one-dimensional input scenario, `rank = 1`, so input `[true, false, true, false, true, false, false, true]` actual output shape is `[4, 1]`, output data is `0 2 4 7`.
