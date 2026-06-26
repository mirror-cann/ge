## Dump Module Overall Design Document

---

### Module Overview

#### Module Boundary

The Dump module writes operator input/output tensor data, overflow information, and exception context to disk during model execution. You use this data for accuracy tuning, problem diagnosis, and performance analysis. The module boundaries include:

- **Functional scope**: The module supports synchronous/asynchronous dump, overflow detection dump, and exception dump. It covers static shape and dynamic shape graphs. The module supports single operator, single stream, multi-stream, and multi-thread scenarios.
- **Interactive components**: The module interacts with GE execution engine, memory management, stream management, HCCL, RTS (Runtime System), ACL interfaces, GE options, and environment variables.
- **Excluded scope**: The module does not handle data parsing (offline tools handle this). The module does not interfere with normal model execution logic.

#### Architecture Layering

The Dump module uses a layered design. You can see the execution flow below:

```text
┌─────────────────────────────────────────────────────────────┐
│                    Entry Configuration Layer                  │
│  DumpManager - Global singleton, manages DumpProperties for multiple sessions  │
│  DumpProperties - Stores dump configuration (path, mode, blacklist, filter list) │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Graph Execution Adapter Layer              │
│  Static graph (RT1.x): DataDumper - davinci_model integration │
│  Dynamic graph (RT2.0): ExecutorDumper - hybrid/rt2 executor integration │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Core Operation Layer                       │
│  DumpOp - Builds OpMappingInfo proto, launches aicpu dump kernel │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Bottom Storage Layer                       │
│  adump interface - Calls RTS-provided low-level interfaces for actual data storage │
└─────────────────────────────────────────────────────────────┘
```

---

### Core Design Principles

1. **Logic reuse with differentiation**: Dump, overflow detection, and exception dump reuse the same data flow and storage framework at the bottom layer. However, you must handle them differently based on trigger conditions, data types, and processing priorities.

2. **Dynamic adaptability**: You can enable or disable dump functionality at runtime. Constraint conditions can refresh in real-time. This avoids restart or graph reconstruction.

3. **Entry consistency**: Environment variables, GE options, and ACL interfaces must behave consistently when enabled. You must explicitly define mutual influences.

4. **Performance preservation**: Enabling dump should not cause significant execution performance degradation (especially in RT2.0 multi-stream multi-thread scenarios). You must not add unnecessary memory allocations or synchronization operations in hot paths.

5. **Boundary determination**: The module provides key logs and interface call traces. This helps you quickly determine whether problems are dump module defects or upper-layer usage issues.

---

### Detailed Design

#### 1. Module Responsibility Division

| Module            | Responsibility                                           | Location                                             |
|-----------------|------------------------------------------------|--------------------------------------------------|
| `DumpManager`   | Global singleton, manages dump configuration for multiple sessions, provides dynamic switch capability | `common/dump/dump_manager.h`                      |
| `DumpProperties`| Stores single session dump configuration (path, dump mode, blacklist, operator filter list, and so on) | `common/dump/dump_properties.h` |
| `DumpOp`        | Builds OpMappingInfo proto, allocates device memory, launches aicpu dump kernel to complete dump | `base/common/dump/dump_op.{h,cc}` |
| `DataDumper`    | RT1.x (static graph) integration, traverses all operators needing dump, collects address information, generates OpMappingInfo for adump | `runtime/v1/common/dump/data_dumper.{h,cc}` |
| `ExecutorDumper`| RT2.0 (dynamic shape) integration, based on Subscriber mechanism, triggers dump before and after node execution | `runtime/v2/subscriber/dumper/executor_dumper.{h,cc}` |
| `ExceptionDumper`| Exception scenario dump, saves exception context                    | `common/dump/exception_dumper.h`                 |

#### 2. Static Graph (RT1.x) Processing Flow (`davinci_model.cc` + `data_dumper.cc`)

During static graph loading, `DavinciModel` completes the following dump-related processing:

```cpp
// During model loading
1. Obtain DumpProperties for current session from DumpManager
2. Determine whether dump is needed, create DataDumper instance if needed
3. Traverse all nodes in model, call SaveDumpTask() for nodes needing dump to save dump information
4. LoadDumpInfo() calls rtDatadumpInfoLoad to send dump information to device
5. After model execution completes, UnloadDumpInfo() cleans up resources
```

**Key design points**:
- HCCL operator processing: HCCL processes according to dynamic logic in static graphs. You need special handling for input/output address retrieval. `DataDumper` supports redirection to original operator address through `ATTR_DATA_DUMP_REF` attribute.
- L1/L1Fusion address processing: For tensors on L1 memory, skip direct dump. Generate OpBuffer only when needed.
- Blacklist filtering: The module supports filtering by operator name and operator type blacklist. This reduces unnecessary dump.

#### 3. Dynamic Graph (RT2.0) Processing Flow (`executor_dumper.cc`)

RT2.0 uses Subscriber mechanism. `ExecutorDumper` as subscriber triggers dump at execution event points:

```text
┌──────────────────┐
│  ModelStart      │ → Init dumper, Update step num, Reset FSM
├──────────────────┤
│  ExecuteStart    │ → Set FSM state, Insert dump before HCCL special handling
├──────────────────┤
│  ExecuteEnd      │ → Fill dump info, DoDataDump, Check overflow
├──────────────────┤
│  ModelEnd        │ → Count iteration, Clear dump debug resources
└──────────────────┘
```

**Key design points**:
- Dependency update mechanism: Since dynamic shape addresses are determined at runtime, `ExecutorDumper` needs to wait for predecessor nodes to complete updates before obtaining correct addresses. The module maintains dependencies through `kernel_idxes_to_dump_units_`. Dump executes only after all dependencies complete updates.
- FFTSPlus support: For FFTSPlus scenarios, the module saves sub-operator information through `ffts_dump_op_`. The module sets this to task info during loading.
- HCCL special handling: HCCL communication operators need to dump input and output before and after execution respectively. `InsertHcclDumpOp()` dumps input at `ExecuteStart` and dumps output at `ExecuteEnd`.
- Overflow detection: After execution ends, the module detects overflow through `rtStreamSynchronizeWithTimeout`. If overflow is detected, overflow dump triggers.

#### 4. `DumpOp` Core Flow (`dump_op.cc`)

`DumpOp` is the core executor for dump operations. It builds proto and launches dump kernel:

```text
LaunchDumpOp()
  ├─ Set dump path, step, model name
  ├─ Get task_id and stream_id from RTS
  ├─ Create Task proto
  ├─ LaunchDump() → DumpInput + DumpOutput based on dump mode
  │    ├─ Blacklist filtering
  │    ├─ Get address, fill shape/dtype/format information
  │    ├─ Add to Task proto
  │    └─ Add to OpMappingInfo
  └─ ExecutorDumpOp()
       ├─ Serialize OpMappingInfo to string
       ├─ Allocate device memory for proto and proto size
       ├─ H2D memcpy
       └─ Launch aicpu kernel "DumpDataInfo"
```

**Key design points**:
- Dynamic address update: The module supports runtime input/output address updates (`UpdateAddrs()`). You use this for dynamic shape scenarios.
- FFTSPlus support: `GenerateFftsDump()` specifically handles FFTSPlus multi-context scenarios. The module generates separate tasks for each context.
- Loop information passing: The module supports passing global_step, loop_per_iter, and loop_cond addresses to dump kernel. This implements step-based dump.

#### 5. `DataDumper` Core Design (`data_dumper.cc`)

`DataDumper` organizes all dump information in static graph scenarios:

- **OpMappingInfo building**: The module organizes all operators needing dump into `OpMappingInfo` protobuf. This includes task, input/output, shape, address, and other information.
- **Address redirection**: Through `ATTR_DATA_DUMP_REF`, the module supports redirecting dump requests to other nodes' input/output. This solves address retrieval problems in AIPP and other scenarios.
- **JSON shape parsing**: For FFTSPlus slice information, the module parses JSON to obtain dynamic shape and calculates correct tensor size.
- **L1fusion special handling**: The module skips direct dump for tensors on L1 memory. The module handles this through `OpBuffer` mechanism.

#### 6. HCCL Differentiated Processing

Static graphs and dynamic graphs handle HCCL differently:

| Scenario     | Processing Method                                                                 |
|----------|--------------------------------------------------------------------------|
| Static graph   | Redirection to original address through `ATTR_DATA_DUMP_REF`. Relies on mapping saved during graph construction       |
| Dynamic graph   | Triggers input dump at `ExecuteStart`, triggers output dump at `ExecuteEnd`. Ensures obtaining correct data after communication |

---

### Function Entry and Configuration Priority

#### Enable Methods

Dump supports three enable methods:

1. **Environment variables**:
   - `DUMP_GE_PATH`: dump output path
   - `DUMP_GE_MODE`: dump mode (input/output/all)
   - `DUMP_GE_LAYER`: specifies operator list needing dump

2. **GE Option**:
   - Set dump-related options through `ge::SetGlobalOption`

3. **ACL interface**:
   - `aclmdlSetDumpConfig`: dynamically sets dump configuration
   - Supports runtime dynamic switch

#### Priority Rules

```text
acl interface > Environment variable > GE Option
```

- If ACL interface explicitly sets dump configuration, ignore GE options and environment variables
- When multiple entries provide configuration simultaneously, use "non-empty override" strategy - later-loaded configuration overrides only explicitly set items, unset items retain previous values
- If environment variables and GE options configure simultaneously and conflict (for example, different dump paths), print ERROR log and select higher priority entry configuration

---

### Multi-Scenario Support

#### Normal Data Dump

- **Trigger timing**: During each iteration execution
- **Data content**: Specified input/output tensors for operators
- **Storage method**: aicpu kernel asynchronous storage

#### Overflow Detection Dump

- **Trigger condition**: `rtStreamSynchronizeWithTimeout` returns overflow error
- **Priority**: Higher than normal dump
- **Design points**:
  - You can use this only after enabling op debug mode
  - Set need_overflow_dump flag after detecting overflow, trigger dump
  - Supports AiCore, AiCpu, FFTSPlus and other kernel types

#### Exception Dump

- **Trigger condition**: When execution exception occurs
- **Data content**: Besides tensor data, includes tiling_data, args, workspace and other context information
- **Design points**:
  - Save uniformly through `ExceptionDumper`
  - Supports Normal and FFTSPlus two processors handling separately
  - Saving exception scene after exception does not affect model exit

---

### RT2.0 Adaptation Key Points

#### Design Challenges

RT2.0 (also called Davinci 2.0) introduces dynamic shape, multi-stream, multi-thread execution model. This brings the following challenges to dump module:

1. **Address dynamism**: Output addresses cannot be determined before execution. You must obtain them after execution
2. **Parallel execution**: Multi-stream multi-thread concurrency requires thread safety guarantee
3. **Subgraph nesting**: Control flow (While/If/Case) causes subgraph nesting. You must correctly handle dependency relationships
4. **FFTSPlus**: Dynamic slicing brings multi-context. Each context needs separate dump

#### Solutions

1. **Dependency update mechanism**: `NodeDumpUnit` maintains `total_update_count` and `cur_update_count`. Dump executes only after waiting for all dependency updates to complete

   ```cpp
   if (++dump_unit->cur_update_count != dump_unit->total_update_count) {
     continue; // Wait for all dependency updates to complete
   }
   ```

2. **Per-node dump unit maintenance**: Each node corresponds to one `NodeDumpUnit`. The module stores updated input/output addresses and shapes
3. **Control flow special handling**: Special handling for exit nodes in While/If subgraphs in `InitOrderHoldersFromControlNodes()`. Correctly establish dependency relationships
4. **FFTSPlus multi-context support**: Save input/output addresses for each context. Generate dump tasks separately

#### Performance Considerations

- **No allocation in hot path**: `ExecutorDumper` pre-allocates all `NodeDumpUnit` during initialization phase. No dynamic allocation during execution phase
- **Lazy address copy**: Perform H2D copy only for host tensors. Device tensors directly use native addresses
- **Minimal synchronization**: Perform stream synchronization only once after dump completes. Does not affect other operator execution

---

### HCCL Processing Key Points

#### Problem Background

In static graphs, HCCL operators are specially handled. You cannot determine their input/output addresses during graph compilation. You need to obtain them at runtime from communication peer end. Dump needs special handling to correctly obtain addresses.

#### Solutions

**Static graph**:
- Save original operator reference through `ATTR_DATA_DUMP_ORIGIN_OP_NAMES` and `ATTR_DATA_DUMP_REF` attributes
- Find actual address based on reference during dump

**Dynamic graph**:
- HCCL operators need to dump input before execution, dump output after execution
- `InsertHcclDumpOp()` triggers dump at `ExecuteStart` and `ExecuteEnd` events respectively
- Ensure input dumps before communication, output dumps after communication completes

---

### Dynamic Switch Support

#### Design Points

- `GlobalDumper` supports registering callbacks. When dump switch state changes, notify all `ExecutorDumper`
- For models containing static subgraphs (`DavinciModelExecute` in RT2.0), support dynamic loading/unloading of dump information:

  ```cpp
  void LoadDumpTaskForDavinciModels(bool dump_enable) {
    for (auto davinci_model : davinci_models) {
      dump_enable ? davinci_model->ReLoadDumpInfo() : davinci_model->UnloadDumpInfo();
    }
  }
  ```

#### Constraints

- Configuration changes take effect in next iteration. Tasks currently executing are not affected
- Dynamic switch does not affect dump information already on device. You need to reload for changes to take effect

---

### Logging and Boundary Determination

#### Logging Specifications

- **Entry logging**: When calling any dump interface (such as `aclmdlSetDumpConfig`), print INFO log containing call stack and parameter summary. Keyword is `dumper`
- **State change logging**: For dump switch state changes, configuration refreshes, circular buffer creation/destruction and other events, print INFO log
- **Error grading**:
  - User configuration errors (such as unwritable path) → WARNING, prompt correct configuration method
  - Dump internal errors (such as memory allocation failure) → ERROR, automatically downgrade (such as drop part of data to continue execution)
  - Data corruption or RTS interface returns exception → ERROR, trigger exception dump to save scene

#### Filename Convention

Each dump file name contains:

```text
[Scenario]_[ModelName]_[OperatorName]_[OperatorType]_[IterationNumber]_[StreamID]_[TaskID]
```

This facilitates quick problem localization.

---

### Constraints and Limitations

1. **RT2.0 does not support watcher mode**: Dynamic shape op currently does not support watcher mode. Outputs warning log and skips

2. **L1 memory does not directly dump**: Tensors on L1/L1Fusion need specific method handling. Does not support direct dump address

3. **Single operator scenario special handling**: Single operator dump does not need step information setting. Goes special path

4. **Empty shape filtering**: Optional outputs with empty shape skip dump. This reduces useless data

---

### Key Design Principles Review

1. **Logic reuse**: Dump, overflow detection, exception dump reuse `DumpOp` and `DataDumper` core logic. Differentiate only in trigger timing and priority

2. **Entry consistency**: Three entries all parse to `DumpProperties` eventually. Core logic does not distinguish entry source

3. **Performance priority**:
   - Prohibit dynamic memory allocation in hot paths
   - Minimize synchronization operations, synchronize only once after dump completes
   - Blacklist filtering skips operators not needing dump as early as possible

4. **Debuggability**:
   - Key paths all have INFO level logs, containing operator name, index, address and other information
   - Error information contains sufficient context for localization

5. **Thread safety**: In RT2.0 multi-thread scenarios, each `ExecutorDumper` maintains its own state. No shared mutable state

---

### Related Files

| File                                       | Description                    |
|--------------------------------------------|-------------------------|
| `base/common/dump/dump_op.{h,cc}`          | DumpOp core implementation          |
| `runtime/v1/common/dump/data_dumper.{h,cc}`| Static graph (RT1.x) dump implementation   |
| `runtime/v2/subscriber/dumper/executor_dumper.{h,cc}` | Dynamic graph (RT2.0) dump implementation |
| `runtime/v1/graph/load/model_manager/davinci_model.cc` | Static graph model loading integrates dump |
| `common/dump/dump_manager.{h,cc}`          | Global dump configuration management        |
| `common/dump/dump_properties.{h,cc}`       | dump configuration storage            |
| `common/dump/exception_dumper.{h,cc}`      | Exception dump implementation            |

---
