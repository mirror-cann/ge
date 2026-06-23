# Constant Folding Feature Analysis

## 1 Feature Overview

Constant folding is a core compilation optimization in GE graph compiler. Its core idea: during graph compilation stage, identify operator nodes with all inputs as constants, complete computation on host side ahead of time, write computation results back as new constant nodes, thereby eliminating unnecessary runtime computation overhead.

GE constant folding is not simply "constant expression evaluation", but forms a complete optimization system containing dimension computation, empty tensor replacement, potential constant marking, delayed effecting mechanisms. It runs through multiple compilation stages including graph preprocessing (Prepare), optimization stage 1 (OptimizeStage1), optimization stage 2 (OptimizeStage2), before auto-fusion (BeforeAutofuse).

### 1.1 Problems Solved

In deep learning computation graphs, many node inputs are determinable at compilation time (like model weights, hyperparameters, Shape-related operations). If these nodes remain to runtime execution on device, two problems arise:

- **Unnecessary computation overhead**: Shape, Rank, Size pure meta-information operations can completely complete at compilation time
- **Block subsequent optimization**: Unfolded constant nodes block dead code elimination, common subexpression elimination optimizations

### 1.2 Functional Scope

Constant folding feature contains following capabilities:

| Capability | Description |
|------------|-------------|
| Standard constant folding | Replace operators with all constant inputs as constant nodes |
| Dimension computation folding | Compile-time evaluation for Shape/Reshape/Transpose dimension operations |
| Dimension adjustment folding | In-place optimize and remove dimension-changing operators like ExpandDims |
| Empty tensor replacement | Replace operators with empty tensor output as empty constant nodes |
| Potential constant marking | Mark nodes that currently have partial non-constant inputs but may become all-constant in future |
| Potential constant effecting | After graph traversal ends, uniformly execute folding on already marked potential constant nodes |

---

## 2 User Usage Scenarios

### 2.1 Offline Compilation Scenario (atc)

When users use atc tool to compile ONNX/PB models as OM files, constant folding defaults to enable at O1 and above optimization levels. Users can control switch through configuration parameters:

```
atc --model=model.onnx --output=model --framework=5 \
    --ge.oo.level=O1 --ge.oo.constantFolding=true
```

Typical benefit scenario: Model contains large amount of auxiliary computations for dynamic Shape inference (like Shape→Gather→Concat→Reshape chain). These can completely pre-compute in static Shape scenario. Constant folding can eliminate them all.

### 2.2 Online Compilation Scenario (aclgrphBuildModel)

When using ACL Graph Builder API to build models, control through `aclgrphBuildInitialize` or `aclgrphBuildModel` configuration parameters:

```cpp
// Global level configuration
std::map<ge::AscendString, ge::AscendString> global_options = {
    {ge::ir_option::OO_LEVEL, "O1"},
    {ge::ir_option::OO_CONSTANT_FOLDING, "true"}
};
aclgrphBuildInitialize(global_options);

// Graph level configuration (higher priority)
std::map<ge::AscendString, ge::AscendString> build_options = {
    {ge::ir_option::OO_CONSTANT_FOLDING, "true"}
};
aclgrphBuildModel(graph, build_options, modelBufferData);
```

### 2.3 Debugging Scenario

When users suspect constant folding causes result abnormality, can disable this optimization for comparison verification:

```cpp
{ge::ir_option::OO_CONSTANT_FOLDING, "false"}
```

After disabling, Size, Shape, ShapeN, Rank operators will not be folded and deleted, still execute on device at runtime. `GeDeletedOp` mechanism in `ge_deleted_op.cc` will give clear error indications for these operators, helping users locate problems.

### 2.4 User Specified Skip Folding

Framework side (like TensorFlow `_grappler_do_not_remove` attribute) or users can prevent specific nodes from being constant folded by setting node attribute `_do_not_constant_folding`. This provides fine control means for scenarios requiring preserving specific nodes.

---

## 3 External Interfaces

### 3.1 Configuration Parameters

| Parameter Key | Parameter Value | Configuration Entry | Description |
|---------------|----------------|--------------------|-------------|
| `ge.oo.constantFolding` | `"true"` / `"false"` | aclgrphBuildInitialize, aclgrphBuildModel, atc | Control constant folding optimization switch |
| `ge.oo.level` | `"O1"` / `"O2"` / `"O3"` | Same as above | Optimization level, O1 and above default enable constant folding |

Parameter definition located at `inc/graph_metadef/external/ge_common/ge_api_types.h`, constant name `OO_CONSTANT_FOLDING`, actual configuration key is `"ge.oo.constantFolding"`.

Option registration located at `base/common/option_register.cc`:

```
REG_OPTION(OO_CONSTANT_FOLDING)
    .LEVELS(OoLevel::kO1)
    .DEFAULT_VALUES({{OoLevel::kO1, "true"}, {OoLevel::kO3, "true"}})
    .CHECKER(OoInfoUtils::IsSwitchOptValueValid)
    .VISIBILITY({OoEntryPoint::kSession, OoEntryPoint::kIrBuild, OoEntryPoint::kAtc})
    .SHOW_NAME(OoEntryPoint::kAtc, "oo_constant_folding", OoCategory::kModelTuning)
```

Option defaults to enable at O1 and O3 optimization levels, supports Session, IR Build and ATC three entry configurations.

### 3.2 Node Attribute Interfaces

| Attribute Name | Function | Setter |
|----------------|----------|--------|
| `ATTR_NO_NEED_CONSTANT_FOLDING` | Mark node no need for constant folding | GE internal Pass |
| `ATTR_NAME_DO_NOT_CONSTANT_FOLDING` | Mark user specified node not do constant folding | Framework Parser / User |
| `ATTR_NAME_POTENTIAL_CONST` | Mark node as potential constant | GE constant folding Pass |
| `ATTR_NAME_POTENTIAL_WEIGHT` | Store potential constant weight value | GE constant folding Pass |
| `ATTR_NAME_POTENTIAL_WEIGHT_INDICES` | Store potential constant output indices | GE constant folding Pass |
| `_is_from_constant_folding` | Mark constant node produced by constant folding | GE constant folding Pass |
| `ATTR_NAME_IS_INSERTED_BY_GE` | Mark node inserted by GE internally | GE internal Pass |

Attribute definition located at `inc/graph_metadef/graph/debug/ge_attr_define.h`.

### 3.3 Tool Class Interface

`GraphOptimizeUtility::ConstantFolding` (located at `compiler/graph/manager/util/graph_optimize_utility.cc`) provides single node level constant folding interface, for other modules (like weight compression judgment `WeightCompressJudge`) to call as needed. This interface sequentially executes ConstantFoldingPass → DimensionComputePass → ReplaceWithEmptyConstPass.

---

## 4 Specific Implementation

### 4.1 Overall Architecture

Constant folding adopts Pass chain execution mode, driven through GE Pass management framework. Core implementation concentrated in `compiler/graph/passes/standard_optimize/constant_folding/` directory, composed of 7 Passes and supporting infrastructure:

```
                          ┌──────────────────────────┐
                          │      BaseNodePass        │
                          │   (compiler/graph/passes  │
                          │    /base_pass.h)          │
                          └──────────┬───────────────┘
                                     │
                          ┌──────────▼───────────────┐
                          │      FoldingPass          │
                          │   (folding_pass.h/cc)     │
                          │  Folding basic operations:│
                          │  Create Const node, delete│
                          │  original node             │
                          │  Reconnect data edges,     │
                          │  preserve control edges    │
                          └──────────┬───────────────┘
                                     │
                     ┌───────────────┼───────────────┐
                     │               │               │
          ┌──────────▼──────┐ ┌─────▼──────────┐ ┌──▼──────────────────┐
          │ PotentialFolding │ │DimensionAdjust │ │ ReplaceWithEmpty    │
          │     Pass         │ │    Pass        │ │    ConstPass        │
          │ (potential_      │ │(dimension_     │ │(replace_with_       │
          │  folding_pass)   │ │ adjust_pass)   │ │ empty_const_pass)   │
          └────────┬────────┘ └────────────────┘ └─────────────────────┘
                   │
        ┌──────────┼──────────┐
        │                     │
 ┌──────▼──────┐  ┌───────────▼──────────┐
 │ Constant    │  │  DimensionCompute    │
 │ FoldingPass │  │     Pass             │
 │ (constant_  │  │ (dimension_compute_  │
 │  folding_   │  │    pass)             │
 │  pass)      │  └──────────────────────┘
 └─────────────┘
```

### 4.2 Pass Inheritance System

Constant folding Pass system adopts three-layer inheritance structure, progressively adding capabilities:

**BaseNodePass** → Defines basic framework for node-level Pass, including node traversal, re-pass (RePass), node deletion tracking mechanisms.

**FoldingPass** → Inherits BaseNodePass, implements core "folding" action (`FoldingPass::Folding`), including:
- Collect folded node downstream anchor connection relationships (`GetIndexAndPeerInDataAnchors`)
- Handle Switch/RefSwitch type input nodes (disconnect data edges, insert Identity node maintain control dependency)
- Create new Const node replace original node output (`AddConstNodeToGraph`)
- Connect new Const node to all downstream consumers of original node
- Isolate and delete original node and its input constant nodes becoming orphaned
- Transfer stream label (Stream Label) attribute to maintain stream planning correctness

**PotentialFoldingPass** → Inherits FoldingPass, introduces "potential constant" concept and computation scheduling mechanism, unified orchestration by `PotentialFoldingPass::Run`.

**ConstantFoldingPass / DimensionComputePass / ReplaceWithEmptyConstPass** → Inherits PotentialFoldingPass, each implements different computation strategies and judgment logic.

**DimensionAdjustPass** → Directly inherits BaseNodePass, handles dimension adjustment operators (like ExpandDims), after completing computation on host side directly isolate and delete, not go through complete constant folding flow.

**PotentialConstTakenEffectPass** → Directly inherits FoldingPass, responsible for uniformly processing all already marked potential constant nodes after graph traversal ends.

### 4.3 Core Pass Details

#### 4.3.1 ConstantFoldingPass — Standard Constant Folding

File: `compiler/graph/passes/standard_optimize/constant_folding/constant_folding_pass.h/cc`

This is constant folding main entry Pass, processing nodes with all inputs as constants. Its execution flow:

1. **Pre-check**
   - Check if user set `_do_not_constant_folding` attribute, if yes then skip
   - Check if node marked `ATTR_NO_NEED_CONSTANT_FOLDING`, if yes then skip
   - Check if node is potential empty constant (all output Shape empty), if yes then hand to ReplaceWithEmptyConstPass handle

2. **Input Verification**
   - Confirm all node inputs are constant nodes (through `OpDescUtils::GetConstInputNodeAndAnchor`)
   - If non-constant input exists but node marked as "potential constant", then set `need_fold_ = false` and return NOT_CHANGED (this round not fold, but preserve mark)
   - Extract weight values from constant input nodes (through `OpDescUtils::GetWeightsFromNodes`)

3. **Memory Priority Strategy Check**
   - When MemoryPriority strategy configured, if input constant node Shape large (>8) and shared by multiple downstream, then skip folding. Because folding copies constant data once, in memory constrained scenario not worthwhile

4. **Computation Execution (Two-level Fallback Strategy)**
   - **First level: AICPU operator kernel** (`ComputeWithHostCpuKernel`)
     - Try through `aicpu_ascend_kernel` engine get operator Host CPU implementation
     - Create operator instance through `OpKernelRegistry`, execute by `HostCpuEngine`
     - This level supports widest operator types, runtime loads `libconstant_folding_ops.so`
   - **Second level: GE built-in kernel** (`ComputeWithBuiltInKernel`)
     - If AICPU does not support this operator, fallback to GE built-in Host Kernel
     - Through `KernelFactory` lookup registered Kernel by operator type (`folding_pass::GetKernelByType`)
     - GE built-in Kernel covers about 40 common operators

5. **Folding Replacement**
   - After computation success, complete graph structure transformation by `FoldingPass::Folding`
   - Newly created Const node will be marked `_is_from_constant_folding=true`, for subsequent flow identification

6. **Performance Statistics**
   - Separately record AICPU kernel and GE built-in kernel folding time and call count
   - Summary output performance trace log in `GraphManager::OptimizeStage1_2`

Pass registration macro:
```
REG_PASS_OPTION("ConstantFoldingPass").SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```
This Pass controlled by `ge.oo.constantFolding` switch.

#### 4.3.2 DimensionComputePass — Dimension Computation Folding

File: `compiler/graph/passes/standard_optimize/constant_folding/dimension_compute_pass.h/cc`

Specifically handles dimension-related computation operations (like Shape, Reshape, Transpose). Difference from ConstantFoldingPass:

- **Only uses GE built-in Kernel**: Get Kernel through `folding_pass::GetKernelByType` and compute, not try AICPU kernel
- **Supports mark but not fold mode**: Can through constructor parameter `need_fold` control whether only do computation marking (in preprocessing stage run in `need_fold=false` mode, only mark potential constant not actually fold)
- Supports coordination with PotentialFoldingPass potential constant mechanism

In preprocessing stage (`GraphPrepare::ComputeConstantShape`), DimensionComputePass runs in `need_fold=false` mode, purpose is first use dimension computation determine Shape information, provide more accurate input for subsequent InferShape.

Pass registration macro:
```
REG_PASS_OPTION("DimensionComputePass").LEVELS(OoLevel::kO1).SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```

#### 4.3.3 DimensionAdjustPass — Dimension Adjustment Folding

File: `compiler/graph/passes/standard_optimize/constant_folding/dimension_adjust_pass.h/cc`

Handles ExpandDims operators adjusting dimension through constant parameters. Workflow:

1. Get node original type, lookup corresponding GE built-in Kernel
2. Check if unknown Shape, unknown Shape nodes skip
3. Call Kernel `Compute(node)` interface execute dimension adjustment computation
4. Delete useless constant axis parameter input nodes
5. Through `IsolateAndDeleteNode` isolate and delete node, and reconnect data edges by `{0}` IO mapping (only preserve data input direct connect data output)

DimensionAdjustPass does not go through complete constant folding replacement flow (does not create Const node), but directly completes dimension inference then simplifies node as direct connect.

Pass registration macro:
```
REG_PASS_OPTION("DimensionAdjustPass").LEVELS(OoLevel::kO1).SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```

#### 4.3.4 ReplaceWithEmptyConstPass — Empty Tensor Replacement

File: `compiler/graph/passes/standard_optimize/constant_folding/replace_with_empty_const_pass.h/cc`

Identifies operators with empty tensor output (Shape contains dimension 0, like `[0, 3, 224, 224]`), replaces as empty constant nodes.

Exclusion rules (node types not replaced):
- Const/ConstantOp/FileConstant constant class nodes
- Data class nodes
- NetOutput nodes
- Control flow operators (Switch, Merge, Enter, NextIteration, Exit, LoopCond etc.)
- Resource operators (Stack, StackPop, StackPush)
- Hcom collective communication operators
- Nodes without output description
- GE internally inserted nodes (`ATTR_NAME_IS_INSERTED_BY_GE` mark)

This Pass also supports `need_fold` parameter control whether only do marking.

Pass registration macro:
```
REG_PASS_OPTION("ReplaceWithEmptyConstPass").LEVELS(OoLevel::kO1).SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```

#### 4.3.5 PotentialConstTakenEffectPass — Potential Constant Effecting

File: `compiler/graph/passes/standard_optimize/constant_folding/potential_const_taken_effect_pass.h/cc`

This is a special Pass, its `Run` method is no-op (directly returns SUCCESS), actual work completes in `OnFinishGraph` callback.

Design intent: During graph traversal, some node partial inputs are not yet constants in current round (like from Shape inference intermediate results). DimensionComputePass etc. will mark as "potential constant" and cache weight. When graph traversal completes (all Pass executed), potential constant inputs may have been folded as constant in previous rounds. At this time PotentialConstTakenEffectPass uniformly scans all nodes marked as potential constant in `OnFinishGraph`, executes delayed folding.

Processing flow:
1. Traverse all nodes in graph, find nodes with `ATTR_NAME_POTENTIAL_CONST` mark
2. Read cached potential weight from attributes (`ATTR_NAME_POTENTIAL_WEIGHT`)
3. If weight exists, call `FoldingPass::Folding` execute folding
4. If weight missing, clear potential constant related attributes and record warning
5. Collect all nodes needing re-pass, pass to next round Pass execution

Pass registration macro:
```
REG_PASS_OPTION("PotentialConstTakenEffectPass").LEVELS(OoLevel::kO1).SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```

### 4.4 Potential Constant Mechanism

Potential constant (Potential Const) is one of GE constant folding core innovations, solving "current inputs not all constant but may become constant in future" scenario.

Related attributes and tool classes:

| Component | Location | Description |
|-----------|----------|-------------|
| `ConstantUtils` | `inc/graph_metadef/graph/utils/constant_utils.h` | Potential constant mark/query/clear tool class |
| `ATTR_NAME_POTENTIAL_CONST` | `inc/graph_metadef/graph/debug/ge_attr_define.h` | Mark node as potential constant |
| `ATTR_NAME_POTENTIAL_WEIGHT` | Same as above | Cache potential constant weight value |
| `ATTR_NAME_POTENTIAL_WEIGHT_INDICES` | Same as above | Mark which output indices are potential constants |
| `_source_pass_of_potential_const` | `potential_folding_pass.cc` | Record source Pass name, prevent cross Pass misoperation |

Mechanism flow:

```
DimensionComputePass / DimensionComputePass
          │
          │ (node partial inputs non-constant)
          ▼
   ComputePotentialWeight()
     → Computation success, get output weight
     → need_fold = false (this round not fold)
          │
          ▼
   UpdatePotentialConstMark()
     → MarkPotentialConstWithPassName()
       → ConstantUtils::MarkPotentialConst()
       → Set _source_pass_of_potential_const
          │
          ▼
   (Multiple rounds InferShape + ConstantFolding after,
    partial inputs folded as constant by other Pass)
          │
          ▼
   PotentialConstTakenEffectPass::OnFinishGraph()
     → Read ATTR_NAME_POTENTIAL_WEIGHT
     → FoldingPass::Folding() execute folding
```

`IsCurPassSameWithSource` method in `PotentialFoldingPass` ensures only Pass originally marking potential constant can update or clear that mark, avoiding interference between different Passes.

### 4.5 Computation Engine

Constant folding computation engine divides into two levels:

#### 4.5.1 GE Built-in Host Kernel

Located at `compiler/host_kernels/`, through `KernelFactory` registration and creation. `Kernel` base class defines three Compute interfaces:

- `Compute(OpDescPtr, inputs, outputs)` — Compute output tensors based on input tensors, for ConstantFoldingPass and DimensionComputePass
- `Compute(NodePtr, outputs)` — Compute output based on node information, some Kernel use
- `Compute(NodePtr)` — Only modify node attributes, for DimensionAdjustPass

Registered Host Kernel distributed by category:

| Category | Directory | Operator List |
|----------|-----------|---------------|
| Array operations | `host_kernels/array_ops/` | Reshape, Squeeze, SqueezeV3, Unsqueeze, UnsqueezeV3, ExpandDims, Rank, Shape, ShapeN, Size, Identity, Empty, BroadcastArgs, BroadcastGradientArgs, GatherShapes |
| Element-wise computation | `host_kernels/elewise_calculation_ops/` | Add, Sub, Mul, FloorDiv, FloorMod, Maximum, Greater, Rsqrt, Cast |
| Selection operations | `host_kernels/selection_ops/` | Slice, SliceD, StridedSlice, GatherV2, Range |
| Transform operations | `host_kernels/transformation_ops/` | Transpose, Permute, Transdata, FlattenV2 |
| Concatenate split | `host_kernels/split_combination_ops/` | ConcatV2, ConcatOffset, Pack, Unpack |
| Padding operations | `host_kernels/pad_ops/` | Fill |
| Reduction operations | `host_kernels/reduce_ops/` | ReduceProd |
| Data flow | `host_kernels/data_flow_ops/` | DynamicStitch |
| Custom | `host_kernels/custom_ops/` | ReFormat, SsdPriorBox |

Each Kernel registers to `KernelFactory` through `REGISTER_COMPUTE_NODE_KERNEL` macro, runtime lookup by operator type.

#### 4.5.2 AICPU Host CPU Engine

As GE built-in Kernel supplement, through AICPU engine execute operators. Located at `runtime/v2/engine/aicpu/` and `runtime/v1/hybrid/node_executor/host_cpu/`.

Load path is `libconstant_folding_ops.so`, provided by OPP (Operator Package), contains wider operator implementations. `compiler/engines/cpu_engine/cpu_engine/constant_folding_stub/constant_folding_ops_stub.cpp` is compilation stub library (no actual implementation), runtime replaced by real OPP library.

`HostCpuEngine` responsible for:
- Loading `libconstant_folding_ops.so` dynamic library
- Creating operator instance through `OpKernelRegistry`
- Calling `RunHostCpuKernel` execute computation

### 4.6 Compilation Flow Integration

Constant folding called in multiple stages of GE graph compilation, forming multiple-round iterative optimization:

```
GraphPrepare::ComputeConstantShape (preprocessing stage)
  │
  ├── ReplaceWithEmptyConstPass (need_fold=false, only mark)
  ├── DimensionComputePass (need_fold=false, only mark)
  ├── ConstantClipPass
  ├── ConstantFoldingPass
  └── InferValueRangePass

GraphManager::OptimizeStage1_2 (optimization stage1)
  │
  ├── ReplaceWithEmptyConstPass
  ├── DimensionComputePass
  ├── ConstantClipPass
  ├── ConstantFoldingPass
  └── DimensionAdjustPass

GraphManager::OptimizeStage2 (optimization stage2, after subgraph merge)
  │
  ├── ConstantFoldingPass
  ├── ReshapeRemovePass
  ├── CondRemovePass
  ├── AssignRemovePass
  └── DimensionAdjustPass

GraphOptimizerBeforeAutofuse (before auto-fusion)
  │
  └── ConstantFoldingPass
```

Multi-stage calling design considerations:
- **Preprocessing stage** runs DimensionComputePass in marking mode, first complete dimension inference then execute standard folding
- **Optimization stage1** is constant folding main battlefield, contains all Pass complete execution
- **Optimization stage2** does constant folding once more for merged subgraph graph, eliminate new constant computations introduced by subgraph merging
- **Before auto-fusion** executes constant folding once more, ensure fusion Pass faces most optimized graph structure

### 4.7 Coordination with Other Optimizations

Constant folding has coordination relationships with multiple other optimization Passes:

- **ConstantClipPass**: Handles weight clipping before constant folding, inserted Min/Max nodes will be folded again by subsequent ConstantFoldingPass
- **DeadCodeElimination**: Orphaned constant nodes produced by constant folding will be cleared by dead code elimination
- **CommonSubexpressionElimination**: Under scenario sharing same constant input, after constant folding common subexpression elimination can further deduplicate
- **WeightCompressJudge**: Weight compression judgment calls `GraphOptimizeUtility::ConstantFolding` first to do constant folding for weight-related nodes before execution
- **DeleteNoConstFoldingFusionPass**: FE quantization Pass, deletes `ATTR_NO_NEED_CONSTANT_FOLDING` attribute on `AscendWeightQuant` operator in fusion stage, enabling it to be constant folding processed
- **DecomposeLargeConstPass**: Large constant split Pass inherits `_is_from_constant_folding` attribute to split new constant nodes
- **AscIrLowerer**: In IR Lowering stage, special handling for control edges from constant folding (allow remove to get more fusion opportunities)
- **CtrlEdgeTransferPass**: Control edge relationships formed after constant folding need to be cleared by control edge transfer Pass