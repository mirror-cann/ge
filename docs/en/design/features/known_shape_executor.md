# GE Static Executor (Known Shape Executor) Feature Analysis

## 1. Feature Background

Static executor is the core component in GE runtime responsible for loading and executing static shape graphs or subgraphs. It uses `DavinciModel` class as carrier, providing two usage modes:

1. **Standalone Static Model Mode**: Entire model compiled to single OM file, directly loaded and executed through `ModelManager`

2. **V2 Runtime Kernel Integration Mode**: In next-generation V2 runtime, static subgraphs embed in execution graph as Kernels, managing lifecycle through Kernel registration mechanism (create, parameter refresh, execute, workspace update), achieving coordinated scheduling with dynamic subgraphs

## 2. User Scenarios

### 2.1 Scenario 1: Offline Model Inference (ACL Mode)

After user converts model to OM file through ATC tool, loads and executes through ACL API on inference side:

```
Model file(.om) → aclmdlLoadFromFile → aclmdlExecute → Output results
```

Underlying execution path: `ModelManager::LoadModelOffline` → `DavinciModel::Init` → `DavinciModel::NnExecute`

### 2.2 Scenario 2: Execution Through GeSession Interface

User loads and executes computational graph through `GeSession` class provided by GE V2 API, this is GE runtime's high-level programming interface.

### 2.3 Scenario 3: V2 Runtime Kernel Integration

In next-generation V2 runtime, static subgraphs integrate into execution graph as Kernels, completing lifecycle management through registered Kernel functions:

- `DavinciModelCreate`: Create and initialize DavinciModel
- `DavinciModelUpdateArgs`: Refresh input/output addresses
- `DavinciModelExecute`: Trigger device-side execution
- `DavinciModelUpdateWorkspaces`: Update workspace base addresses
- `DavinciModelGetRunAddress`: Get runtime memory address (for inter-Kernel address dependencies)

## 3. External Interfaces

### 3.1 DavinciModel Core Interfaces

`DavinciModel` is the core class of static executor, encapsulating model loading, memory management, task distribution and execution flow:

**Loading phase**:
- `Init(ModelParam, outer_fm_mem)`: Model initialization entry, completes memory allocation, resource creation, task sink
- `SetKnownNode(bool)`: Mark whether it's a known shape subgraph node in mixed execution mode

**Execution phase**:
- `NnExecute(stream, async_mode, input_tensor, output_tensor)`: Model execution entry
- `UpdateKnownNodeArgs(inputs, outputs)`: Refresh input/output addresses for known shape subgraphs (mixed execution mode only)
- `CopyModelData`: Copy input data to device side
- `CopyOutputData`: Copy output data back to user buffer

**Resource management**:
- `GetRtModelHandle()`: Get underlying RT model handle
- `GetLogicalMemAllocation()`: Get logical memory allocation table
- `UpdateHbmFmMemBases`: Update HBM feature map memory base addresses

### 3.2 V2 Kernel Registration Interface

```
REGISTER_KERNEL(DavinciModelCreate)       // Create model
REGISTER_KERNEL(DavinciModelCreateV2)     // V2 version create
REGISTER_KERNEL(DavinciModelUpdateArgs)   // Update parameters
REGISTER_KERNEL(DavinciModelExecute)      // Execute model
REGISTER_KERNEL(DavinciModelUpdateWorkspaces) // Update workspace
REGISTER_KERNEL(DavinciModelGetRunAddress)    // Get runtime address
```

## 4. Architecture Design

### 4.1 Class Hierarchy

```
Executor (base/common/model/executor.h)
  ├── ModelExecutor (runtime/v1/graph/execute/model_executor.h)
  │     └── Delegates to ModelManager -> DavinciModel
  │
DavinciModel (runtime/v1/graph/load/model_manager/davinci_model.h)
  ├── Standalone model: known_node_ = false
  └── Known shape subgraph: known_node_ = true
        └── V2 runtime kernel

```

## 5. Core Implementation

### 5.1 Model Loading Flow (DavinciModel::Init)

Model loading is the most complex phase in static executor, involving memory allocation, resource creation, task distribution and other sub-steps.

```
DavinciModel::Init()
│
├── InitRuntimeParams()
│     Extract runtime parameters from GeModel: memory layout, task definitions, stream configurations etc
│
├── InitWeightMem()
│     Load weight data to device-side HBM
│
├── InitFixedFeatureMap()├── InitFixedFeatureMap()
│     Set up fixed (non-refreshable) feature map memory
│     This memory has unchanged addresses during model lifetime
│
├── InitFeatureMapAndP2PMem()
│     Set up refreshable feature map memory
│     Support sub memory management
│
├── PreProcessFileConstants()
│     Process external weight files (FileConstant nodes)
│     Support Combined Weights optimization
│
├── InitIoNodes()
│     Initialize Data and NetOutput nodes
│     Configure Zero-Copy memory mapping
│
├── InitRuntimeResource()
│     Create RT model handle (rtModel_t)
│     Create execution streams (aclrtStream), events (Event), labels (Label)
│
├── TransAllVarData()
│     Transfer Variable data to device side
│
├── InitNodes()
│     Initialize all compute nodes
│     Load TBE Kernel handles, register operator implementation spaces
│
└── DoTaskSink()          ★ Key step: Task sink to device
      │
      ├── BindModelStream()
      │     Bind logical stream to physical RT Stream
      │
      ├── InitTaskInfo()
      │     Create TaskInfo objects from ModelTaskDef
      │     Initialize ModelArgsManager (parameter manager)
      │
      ├── LoadWithQueue()
      │     If queue scheduling is configured, set up queue execution path
      │
      ├── DistributeTask()
      │     Distribute tasks to device side via rtPersistentTaskLaunch
      │     All Kernel launch parameters are preset to device at this stage
      │
      ├── UpdateStaticModelArgsByFm()
      │     Initialize parameter refresh table with feature map addresses
      │
      └── aclmdlRIBuildEnd()
            Mark RT model build complete
```

**Design Significance of Task Sink**: Pre-distribute all tasks generated at compile time to the device side, so execution only needs to trigger `rtModelExecute` without Kernel Launch every time. This is the core guarantee of static executor's high performance - eliminating host-side Kernel launch overhead.

### 5.2 Model Execution Flow (DavinciModel::NnExecute)

```
DavinciModel::NnExecute(stream, async_mode, input_tensor, output_tensor)
│
├── InitModelStream(stream)
│     Set execution stream
│
├── CopyModelData(input_tensor, output_tensor)
│     │
│     ├── UpdateAllNodeArgs()
│     │     Update all Kernel launch parameters
│     │     Including input/output addresses, shape information, etc.
│     │
│     └── CopyInputForNoZeroCopy()
│           For non-zero-copy inputs, execute H2D data copy
│
├── rtModelExecute(rt_model_handle_, rt_model_stream_, 0U)  ★ Device-side execution
│     Or rtModelExecuteSync() (MDC scenario with timeout control)
│
├── rtStreamSynchronizeWithTimeout()
│     Wait for execution completion (built-in stream scenario)
│
├── CopyOutputData(output_tensor)
│     Copy output data back to user buffer
│     Skip this step in zero-copy mode
│
└── UpdateOutputTensorShape()
      Update output tensor shape (dynamic shape scenario)
```

### 5.3 Address Refresh Mechanism (Core Innovation)

In mixed execution mode, the input/output addresses of known shape subgraphs may change with each iteration. The static executor implements efficient address refresh through **logical memory allocation table + active address mapping** mechanism.

#### 5.3.1 Data Structures

`DavinciModel` maintains the following key data structures:

- `logical_mem_allocations_`: Logical memory allocation table, recording each logical memory region's type (INPUT/OUTPUT/FEATURE_MAP), size, hit count and other metadata
- `allocation_ids_to_active_base_addr_`: Active address mapping table, mapping allocation_id to current execution's actual device address
- `refreshable_input_index_and_allocation_ids_`: Mapping from refreshable input index to allocation_id
- `refreshable_output_index_and_allocation_ids_`: Mapping from refreshable output index to allocation_id
- `refreshable_fm_index_and_allocation_ids_`: Mapping from refreshable feature map index to allocation_id

#### 5.3.2 Refresh Flow

```
DavinciModel::UpdateKnownNodeArgs(inputs, outputs)
│
├── ConstructActiveMemBaseAddrsForKnownNode(ret_up, inputs, outputs)
│     │
│     ├── Update FM addresses
│     │     Traverse refreshable_fm_index_and_allocation_ids_
│     │     Write addresses from runtime_param_.fm_memory_infos to active address table
│     │
│     ├── Update input addresses
│     │     Traverse refreshable_input_index_and_allocation_ids_
│     │     Write user-provided inputs[i] device addresses to active address table
│     │     First execution includes non-frozen inputs, subsequent uses zero_copy_no_frozen
│     │
│     └── Update output addresses
│           Traverse refreshable_output_index_and_allocation_ids_
│           Write user-provided outputs[i] device addresses to active address table
│
└── args_manager_.UpdateForExecute(ret_up, rt_model_stream_)
      Copy updated active address table to device side
      Implement efficient refresh through UpdateModelParam Kernel
      ret_up determines refresh strategy (full refresh vs incremental refresh)
```

**Design Sophistication**:

1. **Incremental Refresh Strategy**: The `ret_up` variable records the maximum strategy level that needs refresh, `args_manager_` decides to only copy changed addresses based on this value, minimizing H2D bandwidth consumption
2. **Frozen Input Optimization**: For inputs with unchanged addresses (Frozen Inputs), exclude from refresh list after first execution to avoid unnecessary address updates
3. **Zero-Copy Support**: User-provided I/O buffers directly map to device Kernel parameters, no intermediate copies needed

### 5.4 Impact of known_node_ Flag

`known_node_` is the key flag distinguishing standalone static models from known shape subgraphs in mixed execution mode. After setting this flag (via `SetKnownNode(true)`), `DavinciModel`'s behavior changes as follows:

| Behavior | known_node_ = false | known_node_ = true |
|----------|---------------------|-------------------|
| Session ID retrieval | Use `runtime_param_.graph_id` | Use `runtime_param_.root_graph_id` |
| Address refresh | Use `UpdateAllNodeArgs` | Use `UpdateKnownNodeArgs` |
| Feature map base address | Fixed non-refreshable | Refreshable (`feature_base_refreshable_ = true`) |
| Error tracking cleanup | Execute | Skip |
| Variable initialization | Standard path | Special path |
| Memory segmentation | May merge to single segment | Maintain segment structure |

### 5.5 ModelArgsManager Parameter Management

`ModelArgsManager` is the core component managing Kernel launch parameters in static executor, responsible for:

1. **Initialization Phase**: Parse all task parameter layouts from `ModelTaskDef`, establish mapping from logical addresses to device parameters
2. **Execution Phase**: Update device-side parameters based on active address table, implement efficient refresh through `UpdateModelParam` Kernel
3. **Strategy Management**: Maintain `id_to_policy` mapping, support both full refresh (all-one-time) and incremental refresh strategies

### 5.6 Memory Management Strategy

Static executor adopts hierarchical memory management strategy:

```
Device-side Memory Layout
│
├── Weight memory (weights_mem_base_)
│     Model weight data, fixed address after loading
│
├── Fixed feature map memory (fixed_mem_base_)
│     Non-refreshable feature map memory
│     Address unchanged during model lifetime
│
├── Refreshable feature map memory (mem_base_)
│     Support runtime address refresh
│     In segmented scenario, address of first refreshable FM segment
│
├── Zero-copy I/O memory
│     User-provided input/output buffers
│     Addresses refreshed through args_manager_
│
└── Variable memory (var_mem_base_)
      Model variables (e.g., BatchNorm running mean/var)
```

## 6. Compiler-Side Support

### 6.1 Dynamic/Static Shape Graph Partitioning

`DynamicShapePartitioner` is responsible for partitioning the computational graph into KNOWN_SHAPE and UNKNOWN_SHAPE clusters:

```
DynamicShapePartitioner::Partition()
│
├── MarkUnknownShapeNodes()
│     Mark all nodes containing unknown dimensions (-1) or unknown rank (-2)
│
├── InitClusters()
│     Create Cluster for each node
│     Types include: DATA / KNOWN_SHAPE / UNKNOWN_SHAPE / NETOUTPUT
│
├── MergeClusters()
│     │
│     ├── MergeClustersUnknownShape()
│     │     If two UNKNOWN_SHAPE clusters are connected, merge them
│     │     All KNOWN_SHAPE clusters on merge path also get absorbed
│     │
│     ├── MergeClustersNormal()
│     │     If two KNOWN_SHAPE clusters have only one path between them, merge them│     │
│     └── MergeClustersInputData()
│           Merge all INPUT_DATA clusters
│
└── PruneUniqueClusters()
      Deduplicate merged clusters
```

**Key Constraint of Merge Rules**: UNKNOWN_SHAPE clusters are "contagious" - if there is a path between two unknown shape nodes, all known shape nodes on the path will be marked as unknown shape. This is because known shape nodes' outputs may serve as inputs to unknown shape nodes and require unified management.

### 6.2 Known Shape Graph Compilation

`GraphBuilder::BuildForKnownShapeGraph()` is responsible for compiling KNOWN_SHAPE clusters:

- Generate complete `ModelTaskDef` (containing all task definitions)
- Calculate precise memory allocation scheme (`MemAllocation`)
- Generate zero-copy offset information (`ZeroCopyOffset`)
- Output `GeModel` object, containing compiled graph information and task definitions

## 7. V2 Runtime Kernel Integration

V2 runtime integrates static subgraphs as Kernels into execution graph, providing finer-grained control:

### 7.1 DavinciModelCreate

Create and initialize DavinciModel instance:

1. Get `GeModel` object from input
2. Create `DavinciModel` instance, set `known_node_=true`
3. Set Session ID, Root Graph ID, Step ID and other context information
4. Initialize weight memory and feature map memory
5. Call `DavinciModel::Init()` to complete loading
6. Output DavinciModel pointer to downstream Kernels

### 7.2 DavinciModelUpdateArgs

Refresh input/output addresses before each execution:

1. Get device addresses of input/output Tensors from KernelContext
2. Construct `vector<uint64_t>` address list
3. Call `DavinciModel::UpdateKnownNodeArgs()` to refresh addresses

### 7.3 DavinciModelExecute

Trigger device-side execution:

1. First call `DavinciModelUpdateArgs` to refresh addresses
2. Call `rtModelExecute` to trigger execution
3. Call `CopyOutputData` to copy output data

### 7.4 DavinciModelUpdateWorkspaces

Update workspace base addresses:

1. Get workspace addresses and memory types from KernelContext
2. Call `DavinciModel::UpdateHbmFmMemBases()` to update HBM memory
3. Call `DavinciModel::UpdateExMemBase()` to update other memory types

### 7.5 DavinciModelGetRunAddress

Get runtime memory address (for downstream Kernel address dependencies):

1. Query actual runtime address based on `MemoryBaseTypeOffset` (memory type + offset)
2. Support Weight, FileConstant and other memory types
3. Write address to output Tensor

## 8. Key Design Decision Analysis

### 8.1 Why Choose Task Sink + rtModelExecute Architecture?

**Alternative Comparison**:

| Approach | Host-side Overhead | Device-side Overhead | Flexibility |
|----------|-------------------|---------------------|-------------|
| Kernel Launch per execution | High (Launch every time) | Low | High |
| Task Sink + rtModelExecute | Low (address refresh only) | Low | Medium |
| Compile entire graph to single Kernel | Lowest | Lowest | Low |

GE chose Task Sink + rtModelExecute because:

1. **Compile-time Certainty**: All parameters of KNOWN_SHAPE subgraphs are known at compile time, can safely pre-distribute tasks
2. **Execution Efficiency**: Eliminate host-side Kernel Launch overhead, rtModelExecute only triggers preset task chain
3. **Address Refresh Flexibility**: Implement efficient address refresh through `ModelArgsManager`, support dynamic I/O addresses

### 8.2 Why Introduce known_node_ Flag Instead of Creating New Class?

In mixed execution mode, `DavinciModel` switches behavior through `known_node_` flag instead of creating separate subclasses. Design rationale for this choice:

**Advantages**:
- Maximized code reuse: Core logic like loading, task distribution, execution completely shared
- Low maintenance cost: Only need conditional branches at difference points
- Consistent memory layout: Both modes use same memory management structure

**Trade-offs**:
- Class responsibility not single: One class bears both standalone model and subgraph roles
- Conditional branches increase complexity: `if (known_node_)` judgments scattered in code

Comments in the code also reflect this - the `// todo temporary solution` comment on `UpdateKnownNodeArgs` method indicates that future refactoring may improve this design.

### 8.3 Design Trade-offs in Address Refresh Mechanism

Address refresh mechanism is the core innovation of static executor, facing the following trade-offs in design:

**Full Refresh vs Incremental Refresh**:
- Full refresh: Simple and reliable, but wastes H2D bandwidth
- Incremental refresh: Controlled by `ret_up` strategy level, only refresh changed addresses, but complex implementation

GE chose incremental refresh strategy, recording each allocation's refresh strategy level through `active_mem_base_id_to_plicy` mapping table, deciding actual copied data amount based on `ret_up` during `UpdateForExecute`.

**Zero-Copy vs Intermediate Copy**:
- Zero-copy: User buffers directly mapped, address refresh suffices, no data copy overhead
- Intermediate copy: GE internally manages I/O buffers, requires additional H2D/D2H copies

GE prioritizes zero-copy mode, only falling back to intermediate copy when user buffers don't meet alignment requirements or are device-inaccessible.

## 9. Performance Optimization Points

### 9.1 Task Sink Pre-distribution

All Kernel tasks are pre-distributed to device side at model loading time, no Kernel Launch needed during execution. This is the biggest performance advantage source of static executor compared to dynamic executor.

### 9.2 Incremental Address Refresh

Minimize H2D data transfer per execution through `ModelArgsManager`'s incremental refresh strategy. For Frozen Inputs (inputs with unchanged addresses), no longer participate in refresh after first execution.

### 9.3 Zero-Copy I/O

User-provided device buffers directly map to Kernel parameters, avoiding intermediate copies. Significant benefits in training scenarios and large-batch inference scenarios.

### 9.4 Stream Reuse

Implement Stream reuse through `ReusableStreamAllocator`, reducing Stream creation and destruction overhead. Particularly important in multi-model concurrent loading scenarios.

### 9.5 Shrink Optimization

Call `Shrink()` after model loading completes to release host-side `GeModel` object, reducing memory footprint. Because all necessary information has been distributed to device side, host-side graph structure is no longer needed.

## 10. File List

### Runtime Core

| File Path | Function |
|----------|---------|
| `runtime/v1/graph/load/model_manager/davinci_model.h` | DavinciModel class definition |
| `runtime/v1/graph/load/model_manager/davinci_model.cc` | DavinciModel implementation (approx. 9281 lines) |
| `runtime/v1/graph/load/model_manager/model_manager.h` | ModelManager singleton definition |
| `runtime/v1/graph/load/model_manager/model_manager.cc` | ModelManager implementation |
| `runtime/v1/graph/load/model_manager/model_args_manager.h` | Parameter manager definition |

### V2 Kernel

| File Path | Function |
|----------|---------|
| `runtime/v2/kernel/known_subgraph/davinci_model_kernel.cc` | V2 Kernel integration: Create/Execute/UpdateArgs |

### Compiler

| File Path | Function |
|----------|---------|
| `compiler/graph/partition/dynamic_shape_partition.h` | DynamicShapeCluster/Partitioner definition |
| `compiler/graph/partition/dynamic_shape_partition.cc` | Known/unknown shape graph partitioning logic |

### Base Interfaces

| File Path | Function |
|----------|---------|
| `base/common/model/executor.h` | Executor abstract interface |