# ArgsUpdater Address Refresh Custom Operator Sample

## Sample Overview

- Graph construction entry: `GE`
- Operator programming language: `Ascend C` (RTC runtime compilation)
- Compilation method: The `.cpp` file compiles the host-side custom op, while kernel source code is compiled to device binary through RTC at runtime
- Core pipeline: `Ascend C kernel source code -> RTC runtime compilation -> GE deliverables -> In-process graph construction -> Session::ExecuteGraphWithStreamAsync online execution`
- Difference from other samples: This sample focuses on the `ArgsUpdater` interface working with `MallocReadOnlyDevArgs` to implement address refresh. The GE framework manages args synchronization, avoiding extra D2D copies (MEMCPY_ASYNC) and improving repeated execution performance.

This sample demonstrates the complete pipeline of the `ArgsUpdater` address refresh mechanism: using an Ascend C Add operator with input shape `[4096, 4096]` float32 (16M elements, 64MB), defining two functionally identical operators—`AddRefreshOp` (implements the `ArgsUpdater` interface) and `AddNoRefreshOp` (does not implement it). The performance difference is compared through `Session::ExecuteGraphWithStreamAsync` online execution.

The core concept of `ArgsUpdater`: During model loading, `MallocReadOnlyDevArgs` allocates a read-only kernel args memory region on the device side. For subsequent repeated executions, the `UpdateHostArgs` callback only refreshes address fields in args (input/output tensor pointers). This eliminates the extra D2D copies (MEMCPY_ASYNC) that the GE framework inserts to synchronize operator input/output tensor content to the device side, achieving approximately 1.17x performance improvement in high-frequency execution scenarios.

## Applicable Scenarios

- Understanding the implementation and performance benefits of the `ArgsUpdater` + `MallocReadOnlyDevArgs` address refresh mechanism.
- Viewing the complete process of Ascend C kernel compilation through RTC and invocation in GE custom operators.
- Comparing performance differences between implementations with and without address refresh in high-frequency execution scenarios.

## Prerequisites

### CANN

- The CANN environment is properly installed and configured, for example, by executing `source ${ASCEND_HOME_PATH}/set_env.sh`.
- The current environment has `ACL`, `GE`, and `Graph` related header files and libraries.
- Refer to the [Installation Guide](../../../docs/zh/quick_install.md) to complete toolkit and ops package installation.

### Framework and Plugins

- This sample does not depend on PyTorch, TensorFlow, or TorchAir.
- The kernel source code `add_custom_kernel/add_custom.asc` is compiled through RTC at runtime and does not require pre-compilation.

### Environment Variables

- `ASCEND_HOME_PATH`
- `ASCEND_CUSTOM_OPP_PATH` will be automatically appended to the current sample's `output/` in `run.sh`

### Additional Dependencies

- `cmake`
- `g++`

## Quick Run

Execute in the `examples/custom_op/args_refresh_add_custom` directory:

### Recommended Method

```bash
source ${ASCEND_HOME_PATH}/set_env.sh
bash run.sh
```

`run.sh` automatically completes configure, build, and install, and appends `output/` to `ASCEND_CUSTOM_OPP_PATH`. The script executes the following 2 steps sequentially:

1. Compile custom operator deliverables and executable programs
2. Run `session_run` (online performance comparison)

If successful, the terminal will print output similar to:

```text
[Perf] input shape: [4096, 4096], float32, 64MB
[Perf] iters: 100
[Perf] With    ArgsUpdater: xxx us (avg xxx us/iter)
[Perf] Without ArgsUpdater: xxx us (avg xxx us/iter)
[Perf] Speedup: xxx x
```

### Step-by-Step Method

```bash
source ${ASCEND_HOME_PATH}/set_env.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"

# Online execution: performance comparison
cd build
./args_refresh_session_run
cd ..
```

The command `export ASCEND_CUSTOM_OPP_PATH="$(pwd)/output:$ASCEND_CUSTOM_OPP_PATH"` adds the custom operator package root directory to the environment variable. Then GE loads deliverables according to the rule `output/op_graph/lib/<os>/<arch>/libcust_opapi.so`.

## Directory Structure and Key Files

```text
args_refresh_add_custom
├── CMakeLists.txt
├── README.md
├── run.sh
├── add_custom_kernel
│   ├── add_custom.asc                // Ascend C Add kernel source code (RTC runtime compilation)
│   └── add_custom_kernel.h           // kernel header file
├── ge
│   ├── add_custom.h                  // AddRefreshOp / AddNoRefreshOp proto definition
│   ├── custom_op.cpp                 // Implementation of Execute, ArgsUpdater, InferShape, etc. for both operators
│   └── utils
│       ├── log.h                     // Unified log macros (LOG_ERROR/LOG_WARNING/LOG_INFO)
│       ├── rtc_kernel_loader.h       // RTC kernel loader interface
│       └── rtc_kernel_loader.cpp     // RTC compilation and loading implementation
└── session_run
    └── main.cc                       // In-process graph construction, online performance comparison
```

Key files:

- `ge/custom_op.cpp`
  The core main process of custom operators. `AddRefreshOp` simultaneously implements `EagerExecuteOp`, `ArgsUpdater`, and `ShapeInferOp`; `AddNoRefreshOp` only implements `EagerExecuteOp` and `ShapeInferOp`. Both load kernels through `RtcKernelLoader`, allocate output tensors, and invoke `aclrtLaunchKernelV2` to launch kernels. The difference is that `AddRefreshOp` registers args through `MallocReadOnlyDevArgs` and implements the `UpdateHostArgs` callback, with the GE framework managing args synchronization; `AddNoRefreshOp` does not register args, so the GE framework cannot perceive address changes and must insert extra D2D copies (MEMCPY_ASYNC) to synchronize operator input/output tensor content to the device side during each execution.
- `ge/utils/rtc_kernel_loader.cpp`
  RTC kernel loader, encapsulating the complete pipeline from source code compilation to loading: read kernel source code → `aclrtcCreateProg` → `aclrtcCompileProg` → `aclrtcGetBinData` → `aclrtBinaryLoadFromData` → `aclrtBinaryGetFunction`. Supports dynamically obtaining NPU architecture to generate compilation options.
- `ge/utils/log.h`
  Unified log macros supporting three levels: `LOG_ERROR`, `LOG_WARNING`, and `LOG_INFO`, automatically appending filename and line number.
- `ge/add_custom.h`
  Graph construction operator proto definition, registering `AddRefreshOp` and `AddNoRefreshOp`.
- `add_custom_kernel/add_custom.asc`
  Ascend C Add kernel source code, performing element-wise addition with `BLOCK_SIZE=1024`, compiled through RTC at runtime.
- `session_run/main.cc`
  Constructs two graphs (using `AddRefreshOp` and `AddNoRefreshOp` respectively), executes through `Session::ExecuteGraphWithStreamAsync` and performs 100 rounds of performance comparison. Uses two sets of memory to alternately trigger `UpdateHostArgs` address changes.
- `run.sh`
  Connects the complete pipeline of compilation and online execution.

## Core Pipeline

### Online Execution (Session::ExecuteGraphWithStreamAsync)

1. `session_run/main.cc` constructs two graphs: `refresh_graph` (using `AddRefreshOp`) and `no_refresh_graph` (using `AddNoRefreshOp`), both with input shape `[4096, 4096]` float32.
2. In `ge/custom_op.cpp`, the `Execute` callback compiles and loads the kernel through `RtcKernelLoader` during model loading, and allocates output tensors through `ctx->MallocOutputTensor(...)`. Note: `Execute` is called only once during model loading and will not be invoked again when the model sinks to device execution.
3. `AddRefreshOp` registers the `AddArgs` structure to the GE framework through `ctx->MallocReadOnlyDevArgs(...)` and additionally implements the `UpdateHostArgs` callback: during subsequent executions, the GE framework calls this callback to refresh input/output tensor addresses in host-side args, then the GE framework efficiently synchronizes changes to the device side.
4. `AddNoRefreshOp` does not implement `ArgsUpdater` and does not use `MallocReadOnlyDevArgs` to register args. Although `aclrtMalloc` + `aclrtMemcpy` in Execute only occurs once during model loading, since args are not registered, the GE framework cannot perceive address changes and must insert extra Identity operators in the graph to transfer data, generating extra D2D copies (MEMCPY_ASYNC) during each execution to synchronize operator input/output tensor content to the device side.
5. Both launch Ascend C kernels through `aclrtLaunchKernelV2`.
6. After execution completes, `session_run/main.cc` calculates and prints the total time and speedup ratio for both.

### ArgsUpdater + MallocReadOnlyDevArgs Mechanism

```tree
During model loading (Execute called only once):
  Execute()
    ├─ RtcKernelLoader::Load()                        → RTC compiles and loads kernel
    ├─ MallocReadOnlyDevArgs(&args, sizeof(args))     → Allocates device-side read-only args memory
    ├─ Fill AddArgs { x_ptr, y_ptr, z_ptr }
    └─ aclrtLaunchKernelV2(registered_args)           → Kernel launch

Subsequent executions (AddRefreshOp):
  UpdateHostArgs(ctx)
    ├─ GetKernelArgs(kPlacementHost, 0)               → Get host-side args pointer
    └─ Only refresh args->x_ptr / y_ptr / z_ptr       → Update tensor addresses
  GE framework automatically synchronizes address changes to device side, no need to re-copy args
```

`MallocReadOnlyDevArgs` copies the args structure to the device side and caches it during model loading; for subsequent executions, `UpdateHostArgs` only updates address fields in host-side args, and the GE framework synchronizes changes to the device side, avoiding the extra D2D copies (MEMCPY_ASYNC) that the GE framework inserts to synchronize operator input/output tensor content to the device side.

### RTC Runtime Compilation

```tree
RtcKernelLoader::Load()
  ├─ GetCurrentLibraryDir()                           → Get dynamic library directory
  ├─ LoadTextFromFile(source_path)                    → Read kernel source code
  ├─ GetRtcCompileOption()                            → Dynamically obtain NPU architecture (such as dav-2201)
  ├─ aclrtcCreateProg()                               → Create compilation program
  ├─ aclrtcCompileProg()                              → Compile kernel
  ├─ aclrtcGetBinData()                               → Get compiled binary
  ├─ aclrtBinaryLoadFromData()                        → Load binary
  └─ aclrtBinaryGetFunction()                         → Get function handle
```

RTC compilation completes during model loading, and subsequent executions directly reuse the compiled kernel without re-compilation.

## Build Products

- `output/op_graph/lib/linux/x86_64/libcust_opapi.so`
  Custom operator deliverable used by GE in Linux x86_64 environment; aarch64 environment corresponds to `output/op_graph/lib/linux/aarch64/libcust_opapi.so`.
- `output/op_graph/lib/<os>/<arch>/add_custom.asc`
  Kernel source file, copied by CMake from `add_custom_kernel/` for RTC compilation use.
- `output/op_graph/include/add_custom.h`
  Operator proto header file that can be directly used for graph construction.
- `build/args_refresh_session_run`
  Online execution performance comparison program (`Session::ExecuteGraphWithStreamAsync`).

## Result Validation

When successful, you can observe:

- `output/op_graph/lib/<os>/<arch>/libcust_opapi.so` is generated.
- `output/op_graph/include/add_custom.h` is generated.
- `session_run` terminal output contains `[Perf] Speedup: xxx x`, and `With ArgsUpdater` time is lower than `Without ArgsUpdater`.

If failed, prioritize checking:

- Whether `ASCEND_HOME_PATH` is set and the CANN environment is properly sourced.
- Whether `ASCEND_CUSTOM_OPP_PATH` includes the current sample's `output/`.
- Whether `output/op_graph/lib/<os>/<arch>/libcust_opapi.so` and `output/op_graph/include/add_custom.h` are generated.
- Whether the current environment has an available NPU.

## Precautions / Limitations

- The kernel is compiled through RTC at runtime, with compilation overhead during model loading, and subsequent executions directly reuse it.
- RTC compilation options dynamically obtain NPU architecture through `aclrtGetDeviceInfo`, automatically adapting to different chip models.
- Performance comparison results are affected by NPU model, system load, and other factors; the speedup ratio is for reference only.
- In `session_run`, `ge.graphRunMode` is set to `1` (that is, `PRIORITY_GRAPH` mode), ensuring the online execution pipeline.
- The kernel logic of `AddRefreshOp` and `AddNoRefreshOp` is completely identical; the performance difference comes from the D2D copies (MEMCPY_ASYNC) that the GE framework inserts to synchronize operator input/output tensor content to the device side.
- Performance testing uses two sets of memory for alternate execution, triggering `UpdateHostArgs` address changes, more realistically reflecting optimization effects.

## Appendix

### Operator Specifications

| Item | Content |
| --- | --- |
| Operator type | `AddRefreshOp` / `AddNoRefreshOp` |
| Input | `x`, `y` |
| Output | `z` |
| Input shape | `[4096, 4096]` |
| Output shape | `[4096, 4096]` |
| Input data type | `float32` |
| Output data type | `float32` |
| Format | `ND` |
| kernel name | `add_custom` (Ascend C, RTC runtime compilation) |
| BLOCK_SIZE | `1024` |

### ArgsUpdater Interface Description

| Interface | Class | Purpose |
| --- | --- | --- |
| `EagerExecuteOp::Execute` | `AddRefreshOp` / `AddNoRefreshOp` | During model loading: load kernel, allocate output, allocate device args, launch kernel |
| `ArgsUpdater::UpdateHostArgs` | `AddRefreshOp` | Subsequent executions: get host-side args, refresh tensor address fields |
| `ShapeInferOp::InferShape` | `AddRefreshOp` / `AddNoRefreshOp` | Compile-time output shape inference (same as input) |
| `ShapeInferOp::InferDataType` | `AddRefreshOp` / `AddNoRefreshOp` | Compile-time output dtype inference (same as input) |

### Performance Analysis

Through profiling data analysis, `AddNoRefreshOp` has approximately 323 us extra overhead per round, mainly from the following sources (data below is for reference only; actual time may vary depending on NPU model, system load, and other factors):

| Overhead source | Time | Percentage |
| --- | --- | --- |
| MEMCPY_ASYNC (D2D copy) | ~308 us | 95% |
| Identity operator scheduling overhead | ~15 us | 5% |

Since `AddNoRefreshOp` does not register args through `MallocReadOnlyDevArgs`, the GE framework inserts extra Identity operators to transfer data during graph compilation. These Identity operators generate MEMCPY_ASYNC (D2D copy) during device-side execution, approximately 102 us each time, 3 times per round. In contrast, `AddRefreshOp` refreshes addresses through the `UpdateHostArgs` callback, and the GE framework efficiently synchronizes without needing to insert Identity operators.
