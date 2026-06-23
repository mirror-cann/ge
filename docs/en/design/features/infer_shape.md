# GE InferShape Feature Analysis

## 1 Feature Background

### 1.1 Problem Definition

In deep learning graph compilers, Shape inference (InferShape) is a fundamental capability: given an operator's input Shape, attributes, and possibly input data values, determine the dimension information (Shape) and data type (DataType) of the operator's output tensors.

Shape inference is required in multiple stages:

- **Compilation stage**: Format inference (InferFormat), memory planning (calculating tensor size based on Shape), constant folding
- **Execution stage**: In dynamic Shape scenarios, the runtime must re-infer output Shape based on actual input Shape to allocate memory of the correct size
- **Offline analysis**: Dump model Shape information through the atc tool for debugging and verification

As a graph compiler for Ascend AI processors, GE faces additional complexity in Shape inference: Ascend hardware has affinity preferences for data layout formats (such as NC1HWC0, FZ, and so on). The same tensor has different dimension descriptions from the user perspective and the hardware execution perspective. This requires the InferShape mechanism to maintain two sets of Shape information simultaneously—OriginShape and StorageShape—and use them correctly at different stages.

### 1.2 Design Goals

The GE InferShape mechanism is designed around the following goals:

1. **Semantic correctness**: Accurately reflect the mathematical semantics of the operator; output Shape must be consistent with operator computation results
2. **Format awareness**: Distinguish between user semantic level Shape (OriginShape) and actual storage level Shape (StorageShape), managing both independently
3. **Dynamic Shape support**: Support runtime Shape inference, handling scenarios such as unknown input dimensions (-1), dimension ranges, and symbolic expressions
4. **Compile-runtime consistency**: Use the same operator InferShape registration function during both compilation and runtime to ensure consistent inference results

## 2 OriginShape and StorageShape

### 2.1 Why Two Sets of Shape Are Needed

GE internally describes tensor dimension information using two systems: **Origin** and **Storage**. This is not redundant design, but a necessary requirement of the Ascend hardware architecture.

**Origin** describes information at the user semantic level:

- `OriginFormat`: User-defined data layout format, such as NCHW
- `OriginShape`: Dimension information understood by the user, such as `[8, 3, 224, 224]`

Origin is explicitly provided by the frontend framework or user, directly reflecting user intent without any hardware implementation assumptions. Throughout the compilation process, Origin is always retained as the semantic baseline—any format optimization must proceed without breaking Origin semantics.

**Storage** describes information during actual execution:

- `StorageFormat`: Actual layout in memory, such as NC1HWC0 (splitting C axis into C1 and C0)
- `StorageShape`: Actual dimensions in memory, such as `[8, 1, 224, 224, 16]`

Storage is derived by the GE compiler based on operator capabilities, format affinity, and whole-graph data flow factors. It is an engineering choice oriented toward execution performance.

The relationship between the two can be understood through the following table:

| Perspective | Interface             | Example Content                                  | Description                         |
| ----------- | --------------------- | ------------------------------------------------ | ----------------------------------- |
| Origin      | `GetOriginFormat()`   | NCHW                                             | Format in user semantics            |
| Origin      | `GetOriginShape()`    | [8, 3, 224, 224]                                 | Shape understood by user            |
| Storage     | `GetStorageFormat()`  | NC1HWC0                                          | Format used in actual execution     |
| Storage     | `GetStorageShape()`   | [8, 1, 224, 224, 16]                             | Memory form during execution (with padding) |

The core difference: in NCHW format, the C dimension is 3, but NC1HWC0 format requires C0 dimension to be 16 (hardware alignment), so C1 = ceil(3/16) = 1. This causes StorageShape to change from 4 dimensions to 5 dimensions, with completely different values.

### 2.2 Differences in Inference Timing

**OriginShape is inferred through the InferShape process**. It starts from the computation graph input Shape and infers layer by layer according to operator semantics from front to back, until reaching the graph output. This process is standard practice for graph compilers.

**StorageShape is not an independent inference result**. After OriginShape, OriginFormat, and StorageFormat are all determined, StorageShape is calculated based on the memory layout corresponding to StorageFormat. Specifically, during compilation, this is completed in two steps in `TransferStorageShapeAccordingFormat()` in `graph_prepare.cc`:

1. **Dimension expansion (ExpandDimension)**: Expand origin_shape to an intermediate shape with corresponding dimensions based on reshape_type from origin_format to storage_format
2. **Format conversion (TransferShape)**: Convert the intermediate shape to final storage_shape based on format semantics (such as C axis splitting rules for NC1HWC0)

The same logic also exists in the runtime's `TransformOutputShape()`: the operator InferShape function first writes OriginShape, then the framework automatically completes the conversion to StorageShape.

### 2.3 When to Use Each

| Usage Scenario                            | Shape Used      | Reason |
| ----------------------------------------- | --------------- | ------ |
| Inside operator InferShape function       | OriginShape     | InferShape infers based on operator mathematical semantics, independent of memory layout |
| Format inference (InferFormat)            | OriginShape     | Determine user semantic format |
| Memory allocation size calculation         | StorageShape    | Actual occupied memory size is determined by storage form |
| Execution engine data transfer            | StorageShape    | Need to know the real layout in memory |
| Returning Shape information to user        | OriginShape     | User cares about semantic dimensions |
| Constant folding                          | OriginShape     | Folding logic executes according to mathematical semantics |
| Tiling calculation                        | StorageShape    | Hardware blocking strategy is based on actual storage layout |
| atc dump infershape json                  | OriginShape     | Offline analysis tool for users |

### 2.4 Unified Encapsulation at API Level

In the GE type system, `Shape` is a pure data structure without bound semantics—it can carry either OriginShape or StorageShape, depending on which interface returns it.

The `StorageShape` class (defined in `exe_graph/runtime/storage_shape.h`) is actually a **composite descriptor that carries both Origin and Storage**, although its name may cause confusion:

```
class StorageShape {
    Shape origin_shape_;      // OriginShape information
    Shape storage_shape_;     // StorageShape information
public:
    const Shape &GetOriginShape() const;
    const Shape &GetStorageShape() const;
    Shape *MutableStorageShape();  // Writable storage shape, used for format conversion
};
```

The reason for binding both in the same class is that StorageShape alone cannot uniquely restore semantics. For example, when seeing `[8, 1, 224, 224, 16]`, the C dimension original value could be any value between 1 and 16, and OriginFormat could also differ. Only by carrying both Origin and Storage information can a complete, interpretable description be formed.

## 3 User Usage Scenarios

### 3.1 Single Operator Shape Inference (No Longer Evolving)

When users call `aclopExecuteV2` to execute a single operator, if the operator supports dynamic Shape, they may not know the output Shape in advance. In this case, they can call the `aclopInferShape` interface to obtain the output Shape, then allocate output memory:

```
aclopInferShape(opType, numInputs, inputDesc, inputs, numOutputs, outputDesc, attr)
→ outputDesc is updated in place with inferred Shape, DataType, Format, ShapeRange
→ User allocates output memory based on outputDesc
→ Call aclopExecuteV2 to execute the operator
```

### 3.2 Automatic Inference During Graph Compilation (Offline Compilation/Online Graph Mode)

When users build `ge::Graph` and compile the model through `aclgrphBuildModel`, the GE compiler automatically runs the InferShape Pass during the graph preparation stage, inferring output Shape for all operators. Users do not need to manually call InferShape.

Under optimization level O1, GE disables all graph fusion and UB fusion Passes, but retains basic optimizations such as InferShape, constant folding, and dead edge elimination.

### 3.3 Offline Shape Analysis (atc dump)

Through the atc tool's `--dump_mode=1` parameter, you can parse the model and run Shape inference, serializing the results to a JSON file for debugging and analysis:

```
atc --model=model.onnx --dump_mode=1 --json=infershape.json
```

### 3.4 Runtime Inference in Dynamic Shape Scenarios

In dynamic Shape models, the compilation stage can only determine the Shape range (ShapeRange); the specific Shape value is determined at runtime based on actual input. The runtime engine completes real-time Shape inference through InferShape nodes in the execution graph before executing each operator.

## 4 External Interfaces

### 4.1 C API: aclopInferShape

**Header file**: `acl/acl_op.h`

**Function prototype**:

```c
aclError aclopInferShape(const char *opType,
                         int numInputs,
                         aclTensorDesc *inputDesc[],
                         aclDataBuffer *inputs[],
                         int numOutputs,
                         aclTensorDesc *outputDesc[],
                         aclopAttr *attr);
```

**Function**: Infer output Shape based on operator's input Shape and input values.

**Inference results fall into three cases**:
- Can obtain accurate output Shape → return accurate value
- Cannot obtain accurate Shape but can obtain range → dynamic dimension marked as -1, get range through `aclGetTensorDescDimRange`
- Cannot obtain Shape and range (reserved) → dynamic dimension marked as -2

**Constraint**: If the operator has DYNAMIC_INPUT or DYNAMIC_OUTPUT, you must first call `aclSetTensorDescName` to set input and output names, and the names must be consistent with those defined in the operator IR prototype.

### 4.2 C++ API: Operator::InferShapeAndType

**Header file**: `graph/operator.h`

**Function prototype**:

```c++
graphStatus InferShapeAndType();
```

**Function**: Infer the output Shape and DataType of the Operator. Internally finds and calls the operator's registered InferShape function through OpDesc.

### 4.3 Operator Development Interface: InferShapeContext

**Header file**: `exe_graph/runtime/infer_shape_context.h`

Operator developers implement Shape inference through `InferShapeContext`. This class inherits from `ExtendedKernelContext` and provides the following key interfaces:

| Interface | Description |
| --------- | ----------- |
| `GetInputShape(index)` | Get Shape pointer of the index-th input (read-only) |
| `GetOutputShape(index)` | Get Shape pointer of the index-th output (writable) |
| `GetInputTensor(index)` | Get input Tensor data (for data-dependent Shape inference) |
| `GetComputeNodeInfo()` | Get operator metadata (type, name, I/O description) |
| `GetAttrs()` | Get operator attributes |

Operator registration method:

```cpp
// Method 1: Use InferShapeContext (recommended, v2 interface)
ge::graphStatus InferShapeForCast(InferShapeContext *context) {
    const gert::Shape *shape = context->GetInputShape(0);
    gert::Shape *output_shape = context->GetOutputShape(0);
    *output_shape = *shape;
    return ge::GRAPH_SUCCESS;
}
IMPL_OP(Cast).InferShape(InferShapeForCast);
```

### 4.4 Related Auxiliary Interfaces

| Interface | File | Description |
| --------- | ---- | ----------- |
| `aclSetTensorOriginShape` | `acl/acl_op.h` | Set tensor description's original Shape |
| `aclSetTensorStorageShape` (deprecated) | `acl/acl_op.h` | Set tensor's storage Shape |
| `CtInferShapeContext` | `ct_infer_shape_context.h` | Compile-time InferShape context, extended with `GetInferenceContext()` for resource-type operators |
| `CtInferShapeRangeContext` | `ct_infer_shape_range_context.h` | Compile-time ShapeRange inference context |
| `InferShapeFuncRegister` | Internal operator prototype library | Operator InferShape function registration interface |
| `IMPLEMT_COMMON_INFERFUNC` | Common InferShape macro | Input parameter is Operator base class, supports sharing across multiple operators |
| `BROADCAST_INFER` | Broadcast-type InferShape macro | Broadcast inference based on 2 input Shapes |
| `ELMTWISE_INFER_SHAPEANDTYPE` | Element-wise operator macro | Input Shape = Output Shape |

### 4.5 InferShapeRange Interface

In dynamic Shape scenarios, besides inferring precise Shape, you also need to infer the Shape value range. Type 3 operators must implement InferShapeRange; Type 1 and Type 2 operators can use automatic framework inference when monotonicity conditions are satisfied.

```cpp
ge::graphStatus InferShapeRangeForWhere(InferShapeRangeContext *context) {
    // Set possible minimum and maximum values for each dimension
    context->SetOutputDimRange(0, 0, {0, x_shape_size});
    context->SetOutputDimRange(0, 1, {Rank(x), Rank(x)});
    return ge::GRAPH_SUCCESS;
}
```
- Note: Definition of Type 1, 2, and 3 operators can be found in [op_impl_dev_guide.md](../../user-guides/custom_op/custom_op_v1/op_impl_dev_guide.md)

## 5 Compile-time Implementation

### 5.1 Overall Flow

InferShape executes as a stage of the graph preparation (GraphPrepare) pipeline during compilation. The core call chain is:

```
GraphPrepare::PrepareDynShape()
  └─ FormatAndShapeProcess()
       ├─ InferOriginFormat()            // First round of format inference
       └─ InferShapeForPreprocess()     // InferShape core
            └─ GEPass(names_to_passes)   // BFS topological traversal
                 └─ InferShapePass::Run(node)
```

### 5.2 InferShapePass Class Hierarchy

```
BaseNodePass                     // Base class for all node-level Passes, provides re-pass, suspend/resume mechanism
  └─ InferBasePass               // Shape inference skeleton, coordinates infer + update + peer propagation
       └─ InferShapePass          // Concrete implementation, registered as O1 level Pass
```

`InferShapePass` is registered through `REG_PASS_OPTION("InferShapePass").LEVELS(OoLevel::kO1)` and executes under O1 optimization level.

### 5.3 Pass Orchestration Strategy

InferShapePass does not run independently, but forms a pass chain with multiple related Passes, executing sequentially on each node:

| Pass | Function |
| ---- | -------- |
| AssertPass | Process inference of Assert nodes |
| SwitchDeadBranchElimination | Eliminate dead branches of Switch |
| CondRemovePass | Remove redundant condition nodes |
| MergePass | Process shape merging of Merge nodes |
| **InferShapePass** | **Core Shape inference** |
| ReplaceWithEmptyConstPass | Replace with empty constant |
| SplitShapeNPass | Split ShapeN nodes |
| DimensionComputePass | Dimension calculation |
| ConstantClipPass | Constant clipping |
| ConstantFoldingPass | Constant folding |
| InferValueRangePass | Value range inference |

This interleaved orchestration is key design: constant folding may produce new constant values, triggering downstream nodes to need re-inference of Shape; the re-pass mechanism handles such cascade updates automatically.

### 5.4 Node Traversal and Re-Pass Mechanism

GEPass uses BFS topological traversal. Starting from nodes with in-degree 0, after processing a node, check whether all its successor nodes are ready (all predecessors processed and not suspended), and add ready nodes to the queue.

**Re-Pass mechanism** has three levels:

| Type | Trigger Condition | Behavior |
| ---- | ----------------- | -------- |
| Immediate re-pass | Peer node's TensorDesc changed | Add to queue front, re-execute immediately |
| Deferred re-pass | Operator InferShape function returns `GRAPH_NODE_NEED_REPASS` | Re-process after this BFS round ends |
| Global re-pass | Cross-subgraph resource change causes other graphs to need rebuilding | Mark associated graphs for rebuilding |

### 5.5 Subgraph Shape Propagation

For nodes containing subgraphs (such as IF, CASE, WHILE), Shape propagation has three stages:

1. **Before Subgraph**: Propagate parent node's input TensorDesc to subgraph's Data nodes
2. **Inside Subgraph**: Recursively execute all Passes
3. **After Subgraph**: Collect each subgraph NetOutput's TensorDesc, merge into parent node's output

Merge strategies:
- Standard branch merge: If Shapes are the same, take one; different dimensions set to UNKNOWN_DIM; different ranks set to UNKNOWN_RANK
- Multi-batch scenario: Take the largest Shape among all subgraphs
- Subgraph multi-dimensional scenario: Calculate ShapeRange, differing dimensions marked as UNKNOWN_DIM

### 5.6 V1 Control Flow Handling

V1 control flow (SWITCH/MERGE/LOOPCOND/ENTER/EXIT/NEXTITERATION) requires special handling:

- Exit nodes of While loops are **suspended** during normal traversal to avoid premature propagation when loop body Shape is not yet fully inferred
- After the traversal queue is exhausted, `OnSuspendNodesLeaked()` is used to **resume** Exit nodes one by one, ensuring processing after loop body inference is complete

### 5.7 Resource Context Awareness

Resource-type operators (operators that create/use resources) implement cross-node resource dependency tracking through `ResourceContextMgr` and `InferenceContext`:

1. Operator declares dependent resource keys during InferShape (`RegisterReliedOnResourceKey`)
2. Operator marks changes when modifying resource Shape (`AddChangedResourceKey`)
3. Changes trigger re-inference for all nodes depending on that resource
4. Cross-graph resource changes mark associated graphs for rebuilding through `GraphRebuildStateCtrl`

### 5.8 OriginShape to StorageShape Conversion

InferShapePass only infers OriginShape. StorageShape is calculated in subsequent stages based on OriginShape, OriginFormat, and StorageFormat, in two steps:

1. **Dimension expansion**: Expand OriginShape to intermediate Shape based on reshape_type
2. **Format conversion**: Calculate final StorageShape based on format semantics (such as C axis alignment rules for NC1HWC0)

The conversion function `TransferStorageShapeAccordingFormat()` is located in `compiler/graph/preprocess/graph_prepare.cc`.

## 6 Runtime Implementation

### 6.1 InferShape Nodes in Execution Graph

In dynamic Shape scenarios, the runtime needs to infer output Shape before executing operators. GE achieves this capability by inserting InferShape nodes in the execution graph during the lowering stage.

#### Four Inference Strategies

The runtime supports four InferShape strategies, tried in priority order:

```
InferStorageShape()  Dispatch entry
  ├─ 1. SymbolInferShape        // Symbolic inference (autofuse scenario)
  ├─ 2. Regular InferShape      // Standard v2 inference
  ├─ 3. InferShapeByRule        // Rule-based inference (JSON/compiled binary)
  └─ 4. CompatibleInferShape    // v1 compatible inference
```

| Strategy | Applicable Scenario | Execution Graph Structure |
| -------- | ------------------- | ------------------------- |
| Regular InferShape | Operator has registered v2 infer_shape function | `InferShape(all_shapes, FindInferShapeFunc(node_type, space_registry))` |
| CompatibleInferShape | Operator only has v1 InferShapeFunc | `CompatibleInferShape(CreateOpFromBuffer, FindCompatibleInferShapeFunc(node_type), shapes)` |
| SymbolInferShape | autofuse node (AscBackend, etc.) | `InferShape(symbol_shapes, infer_shape_func)` |
| InferShapeByRule | Operator has attached Shape inference rule | `InferShapeByRule(LoadShapeRule(binary))` |

#### Execution Flow

Taking Regular InferShape as an example, the runtime execution flow:

1. `FindInferShapeFunc` node looks up the operator's infer_shape function pointer from `OpImplSpaceRegistryV2`
2. `InferShape` node takes the function pointer and all input StorageShapes as inputs, calling the operator's infer_shape function
3. Operator InferShape function reads input Shape and writes output OriginShape through `InferShapeContext` interface
4. `TransformAllOutputsShape()` automatically converts output OriginShape to StorageShape (dimension expansion + format conversion)

### 6.2 Execution Graph Optimization

#### FindInferShapeFunc Deduplication

Multiple operators of the same type do not need to repeatedly create `FindInferShapeFunc` nodes. Through `LoweringGlobalData`'s `GetOrCreateUniqueValueHolder()` method, operators of the same optype share one function lookup node, reducing Const node count in the execution graph:

- Without optimization: Each operator node produces `Const(op_type) + FindInferShapeFunc`, N operators of same type produce 2N nodes
- After optimization: N operators of same type share 1 `FindInferShapeFunc` node

#### TrustOutTensor Optimization

When the `trust_shape_on_out_tensor` option is enabled, the `TrustOutTensor` Pass eliminates redundant InferShape nodes:

- If all outputs of an InferShape node connect to OutputData nodes (that is, Shape information can be directly obtained from model outputs), that InferShape node can be deleted
- OutputData nodes directly connect to InferShape's upstream nodes, bypassing Shape inference

#### Pruning

During execution graph optimization stage, Pruner removes useless InferShape-related nodes:
- `FindInferShapeFunc`, `FindInferShapeRangeFunc`, `FindCompatibleInferShapeFunc` are init nodes, can be pruned when no downstream consumers
- `InferShape`, `CompatibleInferShape`, and so on are whitelist nodes, can be pruned when no output edges

### 6.3 Post-inference OriginShape → StorageShape Conversion

Runtime's `TransformOutputShape()` completes the same conversion logic as compile-time:

```
1. ExpandDimsType: Expand OriginShape to dimension count matching StorageFormat
2. ShapeTransferAccordingToFormat: Convert to StorageShape based on format semantics
```

For example, OriginShape `[8, 3, 224, 224]` + OriginFormat NCHW + StorageFormat NC1HWC0 → StorageShape `[8, 1, 224, 224, 16]`.

## 7 Symbolic Shape Inference

### 7.1 Problem and Motivation

In dynamic Shape scenarios, specific Shape values cannot be obtained at compile time (for example, batch_size is determined at runtime). Traditional InferShape works based on specific integer dimensions and cannot handle this situation.

Symbolic Shape inference introduces symbolic variables (such as `s0`, `s1`) and symbolic expressions (such as `s0 * s1 / 2`), completing structural inference of Shape at compile time. At runtime, only substituting actual dimension values into symbolic expressions is needed to obtain specific Shape, without re-executing the operator's InferShape function.

### 7.2 Core Types

| Type | Definition Location | Description |
| ---- | -------------------- | ----------- |
| `ge::Expression` / `ge::Symbol` | `graph/symbolizer/symbolic.h` | Symbolic expression, supports constants, variables, arithmetic operations |
| `gert::SymbolShape` | `exe_graph/runtime/symbolic_shape.h` | Symbolic Shape composed of `Expression` vector |
| `gert::SymbolTensor` | `exe_graph/runtime/symbolic_tensor.h` | Symbolic Tensor (Shape + symbolic data values) |
| `InferSymbolShapeContext` | `exe_graph/runtime/infer_symbol_shape_context.h` | Symbolic inference context |

### 7.3 Inference Flow

```
SymbolicInfoPreProcessor       // Preprocessing: eliminate control flow, fold constants
  ↓
SymbolicShapeSymbolizer::Symbolize  // Symbolize Shape of Data nodes
  ↓                                  // Fixed dimension → constant symbol
  ↓                                  // Dynamic dimension (-1) → variable symbol + Source
SymbolicShapeInference::Infer       // Topological traversal, call symbolic inference function per node
  ↓
SymbolicShapeInference::Simplify    // Simplify all symbolic expressions
  ↓
SymbolicInfoPostProcessor           // Post-processing: mark merge key, symbol count, generate guard function
```

### 7.4 Operator Implementation Examples

Symbolic inference uses `IMPL_OP_INFER_SYMBOL_SHAPE_INNER` macro for registration, and the implementation function receives `InferSymbolShapeContext`:

- **Slice**: Output dimension = `size[i]` or `input_shape[i] - offsets[i]`, offsets and size are read as symbolic data values
- **LayerNorm**: Output 0 has same Shape as input; dimensions after begin_norm_axis for outputs 1 and 2 are set to Symbol(1)
- **Pack**: Insert new dimension Symbol(num) at axis position
- **Reshape**: Target Shape is read from input tensor's symbolic values, when dimension is -1, calculate expression through `in_size / out_size`

### 7.5 Comparison with Regular InferShape

| Aspect | Regular InferShape | Symbolic InferShape |
| ------ | ------------------ | ------------------- |
| Dimension value | Specific integer | Symbolic expression |
| Inference timing | Compile-time / Runtime | Compile-time (autofuse stage) |
| Dynamic Shape | Requires runtime re-inference | Compile-time completes structural inference |
| Data value access | Specific data | Symbolic data values |
| Context | InferShapeContext | InferSymbolShapeContext |

### 7.6 Merge Key Optimization

`MarkInferShapeMergeKey()` generates a deterministic key based on output symbolic Shape for each autofuse node (format like `[dim1_dim2][dim3_dim4]`). During lowering, nodes with the same key can share one InferShape node, reducing runtime overhead.

## 8 aclopInferShape Implementation Mechanism

### 8.1 End-to-End Flow

`aclopInferShape` is the C API exposed by ACL. It completes single-operator Shape inference internally through the following steps:

```
aclopInferShape(opType, numInputs, inputDesc, inputs, numOutputs, outputDesc, attr)
  │
  ├─ Parameter validation (opType, input/output arrays non-empty)
  ├─ Lazy load operator prototype library (from ASCEND_OPP_PATH)
  ├─ Create operator object through OperatorFactory::CreateOperator
  │    └─ Fallback path: when factory registration input count is insufficient, manually construct OpDesc
  ├─ Set operator attributes (write from aclopAttr to OpDesc)
  ├─ Construct Const operator for each input
  │    ├─ Create TensorDesc (shape/format/dtype)
  │    ├─ Copy input data, set as ATTR_NAME_WEIGHTS
  │    ├─ Call InferShapeAndType() on Const operator
  │    └─ Connect to target operator's corresponding input
  ├─ Call InferShapeAndType() on target operator
  │    └─ Dispatch to operator's registered InferShape function
  ├─ Write back results to outputDesc
  │    ├─ Extract shape/dtype/format/range from inferOp.GetOutputDesc(i)
  │    └─ In-place update outputDesc[i]'s dims, shapeRange, dataType, format, name
  └─ Cleanup: disconnect Const operator connections, release temporary data
```

Key design point: `aclopInferShape` does not go through the OpExecutor execution pipeline, but directly constructs `ge::Operator` object and calls `InferShapeAndType()`. The actual Shape inference logic exists in the operator prototype library (`.so` files), and GE is just the dispatcher.

### 8.2 Dump InferShape JSON

The atc tool enables InferShape JSON export through `--dump_mode=1`:

```
atc --model=model.onnx --dump_mode=1 --json=output.json
  │
  ├─ Parse model to ge::Graph
  ├─ GeGenerator::GenerateInfershapeGraph(graph)
  │    └─ Create independent InferShapePass, execute through GEPass
  ├─ DumpInfershapeJson(graph, json_path)
  │    ├─ Serialize to protobuf ModelDef
  │    ├─ Pb2Json::Message2Json convert to JSON
  │    └─ Write to file
  └─ Output JSON contains Shape information of all operators after inference
```

## 9 Key Data Structures

| Structure | Definition Location | Description |
| --------- | ------------------- | ----------- |
| `GeTensorDesc` | `graph/ge_tensor_desc.h` | Tensor descriptor, carries both Origin and Storage information (Shape, Format, DataType, ShapeRange) |
| `GeShape` | `graph/ge_shape.h` | Pure dimension data structure, can carry OriginShape or StorageShape |
| `StorageShape` | `exe_graph/runtime/storage_shape.h` | Origin + Storage composite descriptor |
| `InferShapeContext` | `exe_graph/runtime/infer_shape_context.h` | Context parameter for operator InferShape function |
| `CtInferShapeContext` | `graph/ct_infer_shape_context.h` | Compile-time extended context, adds InferenceContext access |
| `InferSymbolShapeContext` | `exe_graph/runtime/infer_symbol_shape_context.h` | Symbolic inference context |
| `InferenceContext` | `graph/inference_context.h` | Compile-time resource association information (handle shape, marks, etc.) |
| `ResourceContextMgr` | `graph/resource_context_mgr.h` | Resource context manager, tracks resource dependencies |
| `ShapeInferenceRule` | `graph/utils/inference_rule.h` | Shape inference rule based on JSON/compiled binary |

## 10 Key File Index

### API Layer

| File | Description |
| ---- | ----------- |
| `api/acl/acl_op_executor/single_op/acl_op_executor.cpp` | aclopInferShape C interface entry |
| `api/acl/acl_op_executor/single_op/op.cpp` | aclopInferShapeImpl core implementation |
| `api/atc/omg.cc` | DumpInfershapeJson implementation |
| `api/atc/main_impl.cc` | atc command line argument handling |

### Compiler

| File | Description |
| ---- | ----------- |
| `compiler/graph/passes/shape_optimize/infershape_pass.h/.cc` | InferShapePass core implementation |
| `compiler/graph/passes/shape_optimize/infer_base_pass.h/.cc` | InferBasePass base class |
| `compiler/graph/passes/base_pass.h/.cc` | BaseNodePass + GEPass traversal engine |
| `compiler/graph/preprocess/graph_prepare.h/.cc` | Graph preparation pipeline, InferShapeForPreprocess entry |

### Runtime

| File | Description |
| ---- | ----------- |
| `runtime/v2/kernel/common_kernel_impl/infer_shape.h/.cc` | InferShape/FindInferShapeFunc kernel implementation |
| `runtime/v2/graph_builder/bg_infer_shape.h/.cc` | InferShape node construction in execution graph |
| `runtime/v2/lowering/pass/trust_out_tensor.cc` | TrustOutTensor optimization Pass |
| `runtime/v2/lowering/pass/utils/pruner.cc` | Execution graph pruning |

### Symbolic Inference

| File | Description |
| ---- | ----------- |
| `compiler/graph/optimize/symbolic/infer_symbolic_shape/symbolic_shape_inference.h/.cc` | Symbolic inference main flow |
| `compiler/graph/optimize/symbolic/infer_symbolic_shape/symbolic_info_post_processor.cc` | Merge Key and Guard generation |
| `compiler/graph/optimize/symbolic/infer_symbolic_shape/infer/*.cc` | Symbolic inference implementations for various operators |

### Metadata Definitions

| File | Description |
| ---- | ----------- |
| `inc/graph_metadef/exe_graph/runtime/infer_shape_context.h` | InferShapeContext interface definition |
| `inc/graph_metadef/exe_graph/runtime/storage_shape.h` | StorageShape type definition |
| `inc/graph_metadef/exe_graph/runtime/infer_symbol_shape_context.h` | InferSymbolShapeContext interface definition |
| `inc/graph_metadef/graph/symbolizer/symbolic.h` | Expression/Symbol symbolic expression |
