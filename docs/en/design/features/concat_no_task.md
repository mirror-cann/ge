# Concat No Task Feature Analysis

## 1. Feature Background

### 1.1 Problem Scenario

In deep learning models, `ConcatD`/`ConcatV2D` operators concatenate multiple input tensors along a specified dimension into one output tensor. The traditional Concat operator execution flow is:

```
InputA ──┐
InputB ──┼──► Concat Task ──► Output (concatenated result)
InputC ──┘
```

The **Concat Task** requires launching a computation task on Ascend AI Core. It uses DataMove instructions to transport each input data to a continuous memory region of the output address.

### 1.2 Optimization Approach

When Concat operator input tensors are **naturally contiguously arranged** in memory, you do not need to execute any data transportation operations. You can directly reuse the first input address as the output address. The Concat No Task feature identifies this scenario during compilation and marks the Concat operator as a "virtual operator" (Virtual Op), achieving:

- **No hardware Task generation**: Skip AI Core task dispatch
- **No memory transportation**: Output directly reuses input memory address
- **Eliminate redundant computation**: Avoid meaningless data movement

## 2. User Usage Scenarios

### 2.1 Typical Scenario: AllGather + Concat in Distributed Training

In distributed training, multiple cards collect their respective data through AllGather and then concatenate into a complete batch:

```
Card0: Data ──► AllGather ──┐
                              ├──► ConcatD ──► Complete Batch
Card1: Data ──► AllGather ──┘
```

AllGather output data is contiguously arranged in memory by card number order. Concat is logically concatenation, but physically does not require data transportation.

### 2.2 Typical Scenario: Result Merging After Block Computation

Split a large tensor across multiple operators for parallel computation and then merge results:

```
Input ──► Split ──┬──► Relu ──┐
                   ├──► Relu ──┼──► ConcatD ──► Output
                   └──► Relu ──┘
```

When Split splits along batch dimension and each branch computation does not change memory layout, Concat inputs are naturally contiguous.

### 2.3 Applicable Conditions

Concat No Task optimization requires simultaneously satisfying the following strict conditions:

| Condition Category | Specific Requirement |
|-------------------|----------------------|
| Operator type | Only `ConcatD` and `ConcatV2D` |
| Shape constraint | All dimension values before concat_dim axis must be 1 |
| Alignment constraint | concat_dim axis original size must be integer multiple of align_shape corresponding dimension (no padding) |
| Input constraint | Cannot have Scalar input; all input tensor sizes must be 32-byte aligned |
| Source constraint | Cannot have multiple inputs from same output anchor |
| Predecessor node | Cannot be DATA, REFDATA, VARIABLE, CONSTANTOP, CONSTANT node types |
| Predecessor node | Cannot contain subgraph |
| Predecessor attribute | Cannot have continuous_input, continuous_output, reference attributes |
| Successor node | Successor node cannot already be virtual operator (has _no_task attribute) |
| Output constraint | Input cannot simultaneously be model output (NetOutput) |
| Memory type | All input memory types must be consistent |
| LxFusion | Cannot involve LxFusion (L1/L2/UB address, lxslice operator) |
| Shape mode | Does not support Unknown Shape (dynamic Shape) scenario |
| Graph mode | Does not support Single Op scenario and memory non-contiguous allocation scenario |

## 3. External Interfaces

### 3.1 Compilation Period Attribute Marking

Concat No Task interacts with other system modules through the following Graph attributes:

| Attribute Name | Type | Setting Object | Description |
|---------------|------|----------------|-------------|
| `_no_task` | bool | Concat operator OpDesc | Mark this operator does not generate hardware Task |
| `_nopadding_continuous_input` | bool | Concat operator OpDesc | Mark input as continuous memory without padding |
| `_output_reuse_input` | bool | Concat operator OpDESC | Mark output reuses input memory |
| `_reuse_input_on_dim_index` | int64 | Concat operator OpDesc | Specify reused input memory dimension index (fixed as 0) |
| `can_reused_for_concat_optimize` | bool | Predecessor node output TensorDesc | Mark this output is occupied by Concat optimization, cannot be reused by other Concat |

### 3.2 Pass Registration

```
ConcatNotaskPass registered as O3 optimization level GraphPass:
REG_PASS_OPTION("ConcatNotaskPass").LEVELS(OoLevel::kO3);
```

During compilation flow, this Pass runs in `OptimizeStage2` final stage, after subgraph merging completes and before memory conflict handling:

```
graph_manager.cc: OptimizeAfterMergeSubGraph()
  ├── ... (early optimization)
  ├── BufferPoolMemoryPass
  ├── ParallelGroupPass
  └── ConcatNotaskPass  ← Execute after graph stabilizes
```

### 3.3 Runtime Behavior

At runtime (RT1 and RT2), operators marked with `_no_task` receive special handling:

- **RT1 (Davinci)**: `TbeKernelHandle::NeedInit` detects `_no_task` attribute and returns false, skipping Kernel initialization
- **RT2 (V2)**: `IsVirtualOp` function detects `_no_task` attribute, skipping Task generation in AICore Node Converter

## 4. Specific Implementation

### 4.1 Overall Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Compilation Period (Compiler)              │
│                                                             │
│  ┌──────────────────┐    ┌──────────────────────────────┐   │
│  │ ConcatNotaskPass │───►│ Attribute Marking              │   │
│  │                  │    │  _no_task                    │   │
│  │ Verification chain│    │  _nopadding_continuous_input │   │
│  │  ├─ InputCheck   │    │  _output_reuse_input         │   │
│  │  ├─ CheckConcatDim│   │  _reuse_input_on_dim_index   │   │
│  │  ├─ OutputCheck  │    └──────────────────────────────┘   │
│  │  └─ LxFusionCheck│                                       │
│  └──────────────────┘                                       │
└─────────────────────┬───────────────────────────────────────┘
                      │ Attribute transfer
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              Memory Allocation (Memory Assigner)             │
│                                                             │
│  ┌─────────────────────┐    ┌──────────────────────────┐    │
│  │ BlockMemAssigner    │    │ GraphMemAssigner         │    │
│  │                     │    │                          │    │
│  │ Detect NOPADDING_   │    │ Calculate continuous_type │    │
│  │ CONTINUOUS_INPUT    │    │ kTypeInputNoPadding      │    │
│  │                     │    │                          │    │
│  │ no_assign_mem=true  │    │ Calculate nopadding_size │    │
│  │ (no independent     │    │ by dim_index             │    │
│  │ memory allocation)   │    │                          │    │
│  └─────────────────────┘    └──────────────────────────┘    │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              Task Generation (Task Generator)                │
│                                                             │
│  Detect _no_task attribute → Skip this node Task generation │
│  MarkFirstAndLastOps skip notask nodes                      │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              Runtime (Runtime)                               │
│                                                             │
│  RT1: TbeKernelHandle skip initialization                    │
│  RT2: AICoreNodeConverter skip conversion                    │
│  Output address directly reuse first input address            │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 ConcatNotaskPass Core Verification Chain

`ConcatNotaskPass::Run` executes the following verification chain for each ConcatD/ConcatV2D node in the graph. It sets attributes only after all pass:

#### 4.2.1 Scenario Exclusion

- **Single Op scenario**: Single operator mode requires no optimization
- **Memory non-contiguous allocation**: Graph with `ATTR_NAME_MEMORY_DISCONTIGUOUS_ALLOCATION` set skips
- **Unknown Shape**: Operators with dynamic Shape in input or output skip
- **Dynamic Shape graph**: Belonging graph marked with dynamic Shape partition skips

#### 4.2.2 InputCheck (Input Verification)

Check each input anchor sequentially:

1. **IsScalarInput**: Exclude dimension count 0 Scalar inputs
2. **CheckTensorAlign**: In multi-input scenario, each tensor size must be 32-byte aligned
3. **HasSameSourceAnchor**: Through `GetFirstOutAnchorNotInRefNode` trace RefNode chain, ensure no multiple inputs trace back to same output anchor
4. **IsPreNodeTypeValid**: Through `GetFirstNotRefNode` find actual production node, exclude DATA/REFDATA/VARIABLE/CONSTANTOP/CONSTANT
5. **IsPreNodeWithSubgraph**: Predecessor node cannot contain subgraph instance
6. **IsPreOutAnchorCanReuseForConcatOptimize**: Check predecessor output TensorDesc `can_reused_for_concat_optimize` attribute, ensure not occupied by other Concat
7. **IsPreOutAnchorValidMultiRef**: If predecessor output simultaneously connects to NetOutput, cannot optimize
8. **IsPreNodeAttrValid**: Predecessor node cannot have continuous_input, continuous_output, reference, _no_task, _output_reuse_input, _nopadding_continuous_input attributes, nor atomic output
9. **IsSameInputMemType**: All input memory types must be consistent (check through `ATTR_NAME_OUTPUT_MEM_TYPE_LIST`)

#### 4.2.3 CheckConcatDim (Concat Dimension Verification)

This is the core verification logic, ensuring all dimensions before concat_dim axis are 1:

```
Original format (like NCHW) ──► Runtime format (like NC1HWC0)
       │                        │
       │  GetTransferDims()     │
       │  (call FE interface)    │
       ▼                        ▼
   src_to_dst_transfer_dims   dst_to_src_transfer_dims
   {0},{1,4},{2},{3}          {0},{1},{2},{3},{1}
```

- **GetAlignedShape**: Call `transformer::TransferShapeUtils::GetAlignedShape` to get aligned shape
- **GetTransferDims**: Call `transformer::TransferShapeUtils::TransferDims` to get original format to runtime format axis mapping relationship
- **CheckRealConcatDim**: Find actual concat_dim axis in runtime format, verify all dimension values before that axis are 1
- **CheckConcatDimAlignment**: Verify concat_dim axis original size is integer multiple of align_shape corresponding dimension (ensure no padding)

**CheckRealConcatDim key logic**:

- Find real_concat_dim in runtime format through `src_to_dst_transfer_dims[concat_dim][0]`
- If real_concat_dim is produced by axis merging (multiple source axes in `dst_to_src_transfer_dims`), need additional verification:
  - All axis values before real_concat_dim axis are 1
  - All values before concat_dim in merged axis are 1
- If real_concat_dim is not produced by axis merging, only need to verify previous axis values are 1

#### 4.2.4 OutputCheck (Output Verification)

Traverse all successor nodes of Concat node:
- If successor is Reshape and has output node, continue checking Reshape output node
- Successor node cannot already have `_no_task`, `_output_reuse_input`, `_nopadding_continuous_input` attributes

#### 4.2.5 LxFusionCheck (LxFusion Verification)

- **IsLxFusionMem**: Check input/output memory type is L1/L2/UB (on-chip memory used by LxFusion)
- **IsLxFusionOp**: Check operator name contains "lxslice"

### 4.3 Attribute Setting

After all verifications pass, `SetAttrForConcatNotask` executes following operations:

```cpp
// Set Concat operator itself attributes
AttrUtils::SetBool(op_desc, ATTR_NAME_NOTASK, true);
AttrUtils::SetBool(op_desc, ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
AttrUtils::SetBool(op_desc, ATTR_NAME_OUTPUT_REUSE_INPUT, true);
AttrUtils::SetInt(op_desc, ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 0);

// Mark predecessor node output TensorDesc cannot be reused again
for each input:
    AttrUtils::SetBool(output_tensor_desc, 
                       "can_reused_for_concat_optimize", false);
```

### 4.4 Memory Allocation Linkage

#### 4.4.1 BlockMemAssigner

In `AnalyzeSymbolMemReuseInfo`, when detecting node has `ATTR_NAME_NOPADDING_CONTINUOUS_INPUT` attribute:

```cpp
if (is_input_continuous) {
    symbol_mem_reuse_info_[symbol].no_assign_mem_ = true;
}
```

This means this symbol (memory block) will not be allocated independent memory, but reuse upstream memory address.

`GetContinuousNodeLifeTimeBegin` method handles cascaded continuous input scenarios:

```
a   b   c          (first layer)
|   |   |
d   e   f          (second layer)
|___|___|
    |
g   h   i          (third layer, h is continuous input)
|___|___|
    |
    j              (fourth layer, j is continuous input)
```

g cannot reuse memory with a/b/c, because d/e/f memory will be replaced by g address (cascaded continuous input). Therefore g lifetime start needs to trace back to earliest among d/e/f.

#### 4.4.2 GraphMemAssigner

Identify continuous type in `GetContinuousType`:

```
kTypeInputNoPadding = _nopadding_continuous_input && _output_reuse_input
kTypeOutputNoPadding = _nopadding_continuous_output && _output_reuse_input
```

In `GetMemorySize`, for nopadding type:
- Get dimension index through `_reuse_input_on_dim_index`
- Calculate `nopadding_size` (actual data size) and `tensor_size` (complete size with padding)
- Memory allocator allocates precise size memory block based on this

#### 4.4.3 SetInputOutputOffsetPass

For no_task Concat nodes, if has `ATTR_NAME_CONTINUOUS_INPUT` or satisfies BufferFusion condition, will call `SetOutputOffsetForConcat` to set output offset, ensuring output address correctly points to input data starting position.

### 4.5 Task Generation Skip

In `TaskGenerator::MarkFirstAndLastOps`:

```cpp
bool attr_notask = false;
if (ge::AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, attr_notask) && attr_notask) {
    continue;  // Skip notask node, not included in continuous node list
}
```

This ensures no_task nodes are not considered as part of continuous computation nodes in stream, not affecting first and last operator marking.

### 4.6 Runtime Processing

#### 4.6.1 RT1 (Davinci Model)

In `TbeKernelHandle::NeedInit`:

```cpp
bool attr_no_task = false;
const bool get_attr_no_task_flag = AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, attr_no_task);
if (get_attr_no_task_flag && attr_no_task) {
    GELOGI("Node[name:%s, type:%s] does not generate task, skip initialization.", ...);
    return false;  // Skip Kernel initialization
}
```

#### 4.6.2 RT2 (V2 Engine)

In `aicore_node_converter.cc` `IsVirtualOp`:

```cpp
bool attr_no_task = false;
(void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, attr_no_task);
return attr_no_task;  // Return true means virtual operator, skip conversion
```

## 5. Coordination with Other Optimizations

### 5.1 Coordination with Split No Task

Concat No Task frequently works with Split No Task, forming "split-compute-merge" zero-copy pipeline:

```
Input ──► Split(no_task) ──► Computation ──► Concat(no_task) ──► Output
```

Split splits along concat_dim reverse direction, output address directly points to different offsets of input address; Concat treats these offset addresses as continuous memory, directly reuses.

### 5.2 Relationship with Memory Reuse

Concat No Task nodes receive special handling in memory reuse check:

- In `ReuseChecker`, nodes with `_no_task` attribute are considered buffer_pool type
- In `mem_reuse_strategy.cc`, nopadding continuous input nodes participate in special memory reuse strategy

### 5.3 Relationship with Address Refresh

In `MemLayoutConflictUtil`, Concat No Task scenario needs to consider address refresh:

```
data
 |
identity        (insert identity operator to support address refresh)
 |
split(no_task, no_padding_continuous_output)
 /  \
op1  op2
```

When involving user input and requiring address refresh, insert Identity operator between Data and Split.

## 6. Test Verification

Unit test file located at `tests/ge/ut/ge/graph/passes/concat_notask_pass_unittest.cc`, covering following scenarios:

| Test Case | Verification Content |
|-----------|----------------------|
| `allgather_connect_to_concat` | AllGather output connects to Concat, verify attribute setting correct |
| `allgather_connect_to_concat_reshape` | AllGather → Reshape → Concat chain |
| Multiple RefData tests | Verify RefNode chain trace logic |
| Different Format tests | dim verification under ND, NCHW, NC1HWC0 formats |

## 7. Design Considerations

### 7.1 Why Choose Attribute Marking Rather Than Graph Rewriting

Concat No Task uses attribute marking rather than node deletion approach, reasons are:

1. **Preserve debug information**: Dump function needs to preserve operator OpDesc and address information (see `InitNoTaskAndDumpNeededNode`)
2. **Maintain graph structure integrity**: Deleting nodes destroys graph topology relationship, affects other Pass execution
3. **Support dynamic scenarios**: Attribute marking can flexibly handle in different compilation stages

### 7.2 Why Verification Conditions Are So Strict

Concat No Task verification conditions reach more than ten items, because:

1. **Memory safety**: If input is not continuous but marked as no_task, will read wrong data
2. **Alignment constraint**: Ascend hardware has strict requirements on memory alignment, not satisfying 32B alignment may cause hardware exception
3. **Cascading impact**: One no_task node affects downstream memory allocation strategy, wrong marking will propagate

### 7.3 Why Execute at Stage2 End

Comment clearly states "graph stabilized then do ConcatNotaskPass":

- Before Stage2, graph structure may still change (subgraph merging, operator fusion etc.)
- Predecessor node attributes may be modified in subsequent Pass
- Executing at end can ensure judgment based on final stable graph structure