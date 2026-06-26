# GE Custom Operator Development Guide

## 1. Overview

Custom operators allow developers to integrate their own kernels (Ascend C, Triton or other device-side implementations) into GE graph compilation and execution flow, without modifying GE framework code.

**Applicable Scenarios:**

- Frontend framework (PyTorch / TensorFlow) introduces operators not recognized by GE, need to execute on NPU
- Need custom kernel implementation (such as fusion operators, specific optimizations) and integrate into GE compilation and sink flow
- Need to embed third-party kernel binary in GE graph

**Difference from GE Built-in Operators:**

| Dimension | Built-in Operators | Custom Operators |
|------|---------|-----------|
| Registration Method | GE internal REG_OP + engine registration | REG_OP + REG_AUTO_MAPPING_OP, loaded as .so deliverable |
| Build Artifact | Released with GE | Independent .so, loaded via ASCEND_CUSTOM_OPP_PATH |
| Execution Method | GE engine scheduling (DNNEngine etc.) | Framework callback to user-implemented Execute / Compile |
| Update Method | Updated with GE version | Independent from GE, .so can be replaced anytime |

---

## 2. Core Concepts

### 2.1 Interface System

All custom operator implementation classes inherit from `BaseCustomOp`, combining the following capability interfaces as needed:

```
BaseCustomOp (Common Base Class)
├── EagerExecuteOp    → Execute(ctx)          Runtime execution
├── ShapeInferOp      → InferShape(ctx)       Shape inference
│                     → InferDataType(ctx)    DataType inference
├── CompilableOp      → Compile(ctx)          Online compilation
├── PortableOp        → Serialize(buf)        Serialize to OM
│                     → Deserialize(buf)      Deserialize from OM
└── ArgsUpdater       → UpdateHostArgs(ctx)   I/O address refresh
```

Each interface corresponds to an independent callback timing, GE calls corresponding callbacks at specific phases of graph compilation or execution.

### 2.2 Registration Mechanism

Custom operators need two registrations:

**REG_OP** — Define the operator's input, output and attribute proto, used when creating nodes on graph composition side:

```cpp
REG_OP(MyCustomOp)
    .INPUT(x, TensorType({DT_FLOAT, DT_FLOAT16}))
    .INPUT(y, TensorType({DT_FLOAT, DT_FLOAT16}))
    .OUTPUT(z, TensorType({DT_FLOAT, DT_FLOAT16}))
    .OP_END_FACTORY_REG(MyCustomOp);
```

**REG_AUTO_MAPPING_OP** — Map C++ implementation class to op type, GE finds and instantiates by op type name:

```cpp
class MyCustomOp : public EagerExecuteOp { /* ... */ };
REG_AUTO_MAPPING_OP(MyCustomOp);
```

**Naming Constraint**: The op type name in REG_OP, the class name in REG_AUTO_MAPPING_OP, and the op type used on graph composition side must all be consistent.

### 2.3 Deliverables and OPP Package

Custom operators are loaded as `.so` deliverables by GE, path specified via `ASCEND_CUSTOM_OPP_PATH` environment variable. Recommended OPP package directory structure:

```
<custom_opp_root>/
├── op_graph/
│   ├── include/
│   │   └── my_custom_op.h          // REG_OP proto header file
│   └── lib/
│       └── <os>/<arch>/
│           └── libcust_opapi.so     // Deliverable .so
├── framework/                       // Framework-side deliverables (as needed)
│   ├── tensorflow/
│   │   └── npu_supported_ops.json  // TensorFlow scenario
│   └── onnx/
│       └── libonnx_parser_plugin.so // ONNX parser plugin (as needed)
└── bin/
    └── my_kernel.bin                // kernel binary (as needed)
```

Configuration method:

```bash
export ASCEND_CUSTOM_OPP_PATH="<custom_opp_root>:$ASCEND_CUSTOM_OPP_PATH"
```

---

## 3. Scenario Selection Guide

### 3.1 Execution Scenario Decision Table

| Execution Scenario | Recommended Interface Combination | Typical Chain | Reference Sample |
|------|-------------|---------|---------|**Step 2: Implement Execution Logic**

```cpp
class AddCustom : public EagerExecuteOp {
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto *x = ctx->GetInputTensor(0);
    auto *y = ctx->GetInputTensor(1);
    auto *z = ctx->MallocOutputTensor(0, x->GetShape(), x->GetFormat(), x->GetDataType());
    // ... launch kernel with ctx->GetStream() ...
    return GRAPH_SUCCESS;
  }
};
```

**Step 3: Register**

```cpp
REG_AUTO_MAPPING_OP(AddCustom);
```

**Step 4: Compile to .so**

```bash
cmake -S . -B build && cmake --build build -j$(nproc)
```

**Step 5: Configure and Run**

```bash
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/build:$ASCEND_CUSTOM_OPP_PATH"
# Run your graph building/execution program
```

> Complete runnable code see [`examples/custom_op/ascendc_add_custom/`](../../../../../examples/custom_op/ascendc_add_custom/README.md).

---

## 4. Scenario Selection Guide

### 4.1 Decision Table

| Scenario | Recommended Interface Combination | Typical Chain | Reference Example |
|------|-------------|---------|---------|
| Dynamic graph online execution | `EagerExecuteOp` + `ShapeInferOp`(optional) | Graph building → Direct execution | [ascendc_add_custom](../../../../../examples/custom_op/ascendc_add_custom/README.md) |
| Online compilation + Online execution | `EagerExecuteOp` + `CompilableOp` + `ShapeInferOp`(optional) | Graph building → Compile → Execution | — |
| Offline OM model sink | `EagerExecuteOp` + `CompilableOp` + `ShapeInferOp` + `PortableOp` | AIR → ATC → OM → ACL | [compilable_add_custom](../../../../../examples/custom_op/compilable_add_custom/README.md) |
| Data dependent shape | `EagerExecuteOp` + `ShapeInferOp` | Graph building → Execution + shape buffer writeback | [data_dependent_shape_custom](../../../../../examples/custom_op/data_dependent_shape_custom/README.md) |

### 4.2 Scenario A: Dynamic Graph Online Execution

Simplest scenario. Kernel already pre-compiled (Ascend C `.asc` same-library compilation, Triton `.npubin` etc), just need to load and launch in Execute.

```
Graph building → GE callbacks Execute → Get inputs → Allocate outputs → launch kernel → Return
```

**Key Points:**
- Only need to implement `EagerExecuteOp`
- ShapeInferOp optional: If output shape same as input and REG_OP already declared, can omit
- Kernel binary compiles together with .so, or loads from file in Execute

### 4.3 Scenario B: Online Compilation + Online Execution

Kernel source code needs to be compiled to device binary through RTC (Runtime Compilation) during GE compilation phase.

```
Graph building → GE callbacks Compile (RTC compiles kernel)
      → GE callbacks Execute (load binary, launch kernel)
```

**Key Points:**
- In Compile callback can get input's shape, dtype, format etc complete metadata through `ctx->GetInputTensor(i)`
- Suggest generating binary key by shape, support multi-shape / multi-kernel management
- Compile product cached in implementation class member variables, for Execute use

### 4.4 Scenario C: Offline OM Model Sink

On top of Scenario B, need to serialize compile product to OM model file, later deserialize recovery when loading OM.

```
Graph building → ATC offline compilation → Callback Compile (RTC compilation)
                     → Callback Serialize (write binary to OM)
      → ACL loads OM → Callback Deserialize (restore binary from OM)
                     → Callback Execute (launch kernel)
```

**Key Points:**
- Must implement `PortableOp`, Serialize/Deserialize buffer format is user-defined, GE only transparently passes without parsing
- ShapeInferOp is called during OM compilation phase, used to infer output shape/dtype
- Support multi-binary serialization (managed by key)

### 4.5 Scenario D: Data Dependent Shape (Type-3 Operator)

Output shape depends on runtime input data (like Where, NonZero), compilation phase can only determine upper bound.

```
InferShape → Output shape upper bound (contains unknown dimensions)
Execute    → Allocate output by upper bound + allocate shape buffer
           → launch kernel (kernel writes actual shape to shape buffer)
           → stream sync → D2H read back shape buffer → update output shape
```

**Key Points:**
- Use `-1` in InferShape to represent unknown dimension
- In Execute allocate output by max shape, additionally allocate shape buffer (through `MallocWorkSpace`)
- After kernel execution need to self-synchronize stream and read back shape buffer
- Shape buffer format: `[ndim, d0, d1, ...]` (uint64_t array)

> Complete code see [`examples/custom_op/data_dependent_shape_custom/`](../../../../../examples/custom_op/data_dependent_shape_custom/README.md).

---

## 5. Interface Details

### 5.1 EagerExecuteOp::Execute

Runtime execution callback, is the core interface.

```cpp
graphStatus Execute(gert::EagerOpExecutionContext *ctx) override;
```

**Context API:**

| Method | Purpose |
|------|------|
| `ctx->GetInputTensor(index)` | Get input Tensor (contains addr, shape, dtype, format) |
| `ctx->MallocOutputTensor(index, shape, format, dtype)` | Allocate output Tensor device memory |
| `ctx->MallocWorkSpace(size)` | Allocate workspace device memory |
| `ctx->GetStream()` | Get execution stream |
| `ctx->GetOutputTensor(index)` | Get already allocated output Tensor |
| `ctx->MakeOutputRefInput(out_idx, in_idx)` | Output references input address (zero-copy) |

**Typical Flow:**

```cpp
graphStatus Execute(gert::EagerOpExecutionContext *ctx) {
  auto *x = ctx->GetInputTensor(0);
  auto *y = ctx->MallocOutputTensor(0, x->GetShape(), x->GetFormat(), x->GetDataType());
  void *stream = ctx->GetStream();
  LaunchMyKernel(x->GetAddr(), y->GetAddr(), stream);
  return GRAPH_SUCCESS;
}
```

**Notes:**
- Output memory lifecycle managed by context, caller doesn't need to free
- `GetInputTensor` returns `const Tensor*`, cannot modify input data
- For pre-compiled kernel binary scenario, need to load binary in Execute and launch through `aclrtLaunchKernelWithHostArgs`

### 5.2 ShapeInferOp::InferShape / InferDataType

Compile-time Shape and DataType inference callbacks.

```cpp
graphStatus InferShape(gert::InferShapeContext *ctx) override;
graphStatus InferDataType(gert::InferDataTypeContext *ctx) override;
```

**InferShapeContext API:**

| Method | Purpose |
|------|------|
| `ctx->GetInputShape(index)` | Get input origin Shape (`const Shape*`) |
| `ctx->GetOutputShape(index)` | Get modifiable output Shape (`Shape*`) |
| `ctx->GetInputTensor(index)` | Get input Tensor (can access data) |

**InferDataTypeContext API:**

| Method | Purpose |
|------|------|
| `ctx->GetInputDataType(index)` | Get input DataType |
| `ctx->SetOutputDataType(index, dtype)` | Set output DataType |

**Typical Flow:**

```cpp
graphStatus InferShape(gert::InferShapeContext *ctx) {
  auto *in_shape = ctx->GetInputShape(0);
  auto *out_shape = ctx->GetOutputShape(0);
  *out_shape = *in_shape;
  return GRAPH_SUCCESS;
}

graphStatus InferDataType(gert::InferDataTypeContext *ctx) {
  return ctx->SetOutputDataType(0, ctx->GetInputDataType(0));
}
```

### 5.3 CompilableOp::Compile

Online compilation callback, called during GE/ATC compilation phase.

```cppgraphStatus Compile(gert::OpCompileContext *ctx) override;
```

**OpCompileContext API:**

| Method | Purpose |
|------|------|
| `ctx->GetInputTensor(index)` | Get compile-time input Tensor (contains shape, dtype, format) |
| `ctx->GetOutputTensor(index)` | Get compile-time output Tensor |
| `ctx->GetOption(key, value)` | Get compile options |
| `ctx->GetPlatformInfos(...)` | Get target platform information (chip model etc) |

**Typical Flow:**

```cpp
graphStatus Compile(gert::OpCompileContext *ctx) {
  auto *input = ctx->GetInputTensor(0);
  auto key = BuildBinaryKey(input->GetShape());
  auto source = LoadFile("my_kernel.cpp");

  aclrtcProg prog;
  aclrtcCreateProg(&prog, source.c_str(), "kernel_name", 0, nullptr, nullptr);
  aclrtcCompileProg(prog, num_options, options);

  size_t bin_size;
  aclrtcGetBinDataSize(prog, &bin_size);
  std::vector<uint8_t> binary(bin_size);
  aclrtcGetBinData(prog, binary.data());
  device_elves_[key] = std::move(binary);
  aclrtcDestroyProg(&prog);
  return GRAPH_SUCCESS;
}
```

### 5.4 PortableOp::Serialize / Deserialize

Serialization/deserialization callback, used for OM model sink.

```cpp
graphStatus Serialize(std::vector<uint8_t> &buffer) override;
graphStatus Deserialize(const std::vector<uint8_t> &buffer) override;
```

**Key Points:**
- Buffer format completely user-defined, GE doesn't parse only transparently passes
- Serialize called during ATC compilation phase, writes Compile product to buffer
- Deserialize called during OM load execution phase, restores Compile product from buffer
- Multi-binary scenario suggest serialization format: `[count][key_len][key][bin_len][bin]...`

### 5.5 ArgsUpdater::UpdateHostArgs

Callback when I/O address changes, used to refresh address references in kernel args.

```cpp
graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override;
```

**Context API:**

| Method | Purpose |
|------|------|
| `ctx->GetKernelArgs(placement, index)` | Get kernel args at specified location |
| All methods inherited from EagerOpExecutionContext | Get input/output Tensor etc |

**Applicable Scenario:** After static graph sink, when model loads I/O address may differ from compile time, need to refresh addresses in args buffer.

---

## 6. Frontend Access

### 6.1 GE Native Graph Building

When GE native graph building, graph building side needs to be able to see REG_OP proto header file, and create op type consistent with REG_AUTO_MAPPING_OP registration name in graph.

```cpp
auto op = ge::OperatorFactory::CreateOperator("my_node", "AddCustom");
op.SetInput("x", input_x);
op.SetInput("y", input_y);
graph.AddOp(op);
```

### 6.2 PyTorch + TorchAir Graph Entry

PyTorch scenario needs deliverables from both sides:

```
Python Side                              GE Side
┌─────────────────────┐               ┌──────────────────────┐
│ TORCH_LIBRARY       │               │ REG_OP + REG_AUTO_   │
│ (Define operator    │   converter   │ MAPPING_OP           │
│  signature)         │ ──────────→   │ (Deliverable .so)    │
│                     │               │                      │
│ TORCH_LIBRARY_IMPL  │               │                      │
│ (Register PrivateUse1│              │                      │
│  / Meta / XLA key)  │               │                      │
└─────────────────────┘               └──────────────────────┘
```

**Python Side Needs:**
1. `TORCH_LIBRARY` defines operator schema
2. `TORCH_LIBRARY_IMPL` registers PrivateUse1 (NPU execution), Meta (shape inference), XLA (graph compilation) etc keys
3. `@torchair.register_fx_node_ge_converter` maps PyTorch FX node to GE custom operator

```python
@torchair.register_fx_node_ge_converter(torch.ops.my_ops.my_op.default)
def convert_my_op(x, y, meta_outputs=None):
    return torchair.ge.custom_op("AddCustom", inputs={"x": x, "y": y}, outputs=["z"])
```

> Complete code see [`examples/custom_op/ascendc_add_custom/`](../../../../../examples/custom_op/ascendc_add_custom/README.md).

### 6.3 TensorFlow Graph Entry

TensorFlow scenario needs:
1. TensorFlow side custom operator `.so` (declares operator visibility)
2. GE side deliverable `.so` (only needs `REG_AUTO_MAPPING_OP`, no need to additionally provide `REG_OP`)
3. `npu_supported_ops.json` (for TensorFlow Adapter recognition)

```
TensorFlow Side                    GE Side
┌──────────────────┐            ┌────────────────────────────┐
│ libcustom_ops.so │            │ libcust_opapi.so           │
│ (Operator        │  Adapter   │ (REG_AUTO_MAPPING_OP)      │
│  declaration)    │ ────────→  │                            │
│                  │            │ GE proto auto generated     │
│ npu_supported_   │            │ from TF prototype          │
│ ops.json         │            │ No need to write REG_OP    │
└──────────────────┘            └────────────────────────────┘
```

**Difference from GE Native Graph Building**: In TensorFlow scenario, `REG_AUTO_MAPPING_OP` can auto generate GE operator prototype from TF operator prototype (inputs, outputs, attributes), developer doesn't need to additionally write `REG_OP` definition. This further lowers TensorFlow custom operator access burden.

> Complete code see [`examples/custom_op/triton_add_custom/`](../../../../../examples/custom_op/triton_add_custom/README.md).

### 6.4 ONNX Graph Entry

ONNX frontend differs from PyTorch / TensorFlow: GE's ONNX Parser unrecognizable op types will directly error (`PARAM_INVALID`), therefore need to additionally write an **ONNX parser plugin**, maps ONNX node attributes to GE operator attributes.

```
ONNX Model                         ONNX Parser Plugin               GE Side
┌──────────────────┐            ┌────────────────────────┐       ┌──────────────────────┐
│ NodeProto        │            │ REGISTER_CUSTOM_OP     │       │ REG_OP + REG_AUTO_   │
│ (op_type,        │  dlopen    │ (OriginOpType mapping  │       │ MAPPING_OP           │
│  attributes)     │ ────────→  │  ParseParamsFn parse)  │ ───→  │ (Deliverable .so)    │
└──────────────────┘            └────────────────────────┘       └──────────────────────┘
```

**ONNX Operator Type Identifier Format:** `"domain::version::OpType"` (like `"ai.onnx::11::Conv"`, `"com.example::1::MyOp"`). Parser will look up mapping in OpRegistry by this format, not found then parse fails.

**ONNX Parser Plugin Needs:**

1. Use `REGISTER_CUSTOM_OP` to register ONNX op type to GE op type mapping
2. Implement `ParseParamsFn`, extract attributes from ONNX `NodeProto` and set to GE `Operator`

```cpp
#include "register/register.h"
#include "proto/onnx/ge_onnx.pb.h"

// Attribute parse function: maps ONNX NodeProto attributes to GE Operator attributes
static domi::Status ParseMyOp(const google::protobuf::Message *op_src, ge::Operator &op_dest) {
  auto *node = dynamic_cast<const ge::onnx::NodeProto *>(op_src);
  for (const auto &attr : node->attribute()) {
    if (attr.name() == "alpha") {
      op_dest.SetAttr("alpha", attr.f());
    }
  }
  return domi::SUCCESS;
}

// Register: maps ONNX op type to GE op type
REGISTER_CUSTOM_OP("MyCustomOp")
    .FrameworkType(domi::ONNX)
    .OriginOpType({"ai.onnx::11::MyOp", "ai.onnx::13::MyOp"})
    .ParseParamsFn(ParseMyOp);
```

**REGISTER_CUSTOM_OP Key Methods:**

| Method | Purpose |
|------|------|
| `.FrameworkType(domi::ONNX)` | Declare as ONNX framework plugin |
| `.OriginOpType("domain::ver::Type")` | Specify ONNX op type (supports list, covers multi-version) |
| `.ParseParamsFn(fn)` | Attribute parse function (`Message* → Operator&`) |
| `.ParseParamsByOperatorFn(fn)` | Auto-map path attribute parse (`Operator → Operator`) |
| `.ParseOpToGraphFn(fn)` | Expand single operator to subgraph (optional) |
| `.ParseSubgraphPostFn(fn)` | Post-processing for operators with subgraph (optional) |

**Deliverable Organization:** ONNX scenario needs two .so:

| Deliverable | Responsibility | Placement Path |
|--------|------|---------|
| ONNX parser plugin .so | `REGISTER_CUSTOM_OP` + `ParseParamsFn`, responsible for ONNX → GE attribute mapping | Plugin directory under `ASCEND_CUSTOM_OPP_PATH` |
| GE custom operator .so | `REG_OP` + `REG_AUTO_MAPPING_OP` + execution logic | `op_graph/lib/<os>/<arch>/` |

**Notes:**
- ONNX parser plugin .so must be loaded through `ASCEND_CUSTOM_OPP_PATH` before ATC compilation, otherwise Parser cannot recognize custom op type
- When domain in `OriginOpType` is empty defaults to `"ai.onnx"`, custom domain needs explicit specification
- If ONNX operator can map to combination of existing GE operators, can expand to subgraph through `ParseOpToGraphFn`, no need to write GE side custom operator .so

---

## 7. Build and Deploy### 7.1 CMake Build Configuration

Key build elements:

```cmake
# Header file paths (must include)
target_include_directories(my_op PRIVATE
    ${ASCEND_HOME_PATH}/include
    ${ASCEND_HOME_PATH}/include/graph
    ${ASCEND_HOME_PATH}/include/register
    ${ASCEND_HOME_PATH}/include/external
)

# Link libraries (choose as needed)
target_link_libraries(my_op PRIVATE
    ascendcl acl_rt ge_compiler ge_executor
)

# Output .so
add_library(my_op SHARED custom_op.cpp)
```

**PyTorch Scenario Extra Dependencies:** Need to link `torch_npu`, `c10`, `lowering` etc, and include torch_npu header file paths.

**Ascend C Kernel Same-Library Compilation:** Use `find_package(ASC REQUIRED)` and add `.asc` files to `add_library` source file list.

### 7.2 OPP Package Directory Structure

Build products need to be installed according to OPP package specification:

```
output/                              # ASCEND_CUSTOM_OPP_PATH points to this directory
├── op_graph/
│   ├── include/my_op.h              # proto header file (for graph building side use)
│   └── lib/linux/x86_64/
│       ├── libcust_opapi.so         # Deliverable
│       └── my_kernel.cpp            # kernel source code (RTC scenario needs)
└── framework/tensorflow/
    └── npu_supported_ops.json       # TensorFlow scenario needs
```

### 7.3 ATC Offline Compilation Flow

Complete chain for offline OM sink:

```bash
# 1. Build graph and export AIR
./graph_build_program  # Generates model.air

# 2. ATC offline compilation
atc --model=model.air --framework=1 --output=model --soc_version=Ascend910B1

# 3. ACL load execution
./model_exec_program model.om
```

**Prerequisite:** `ASCEND_CUSTOM_OPP_PATH` already points to OPP package root directory containing deliverables. ATC compilation will auto load .so and callback Compile and Serialize.

---

## 8. Debugging and Common Issues

### 8.1 Development Checklist

- [ ] Operator type name, registration class name, graph building side op type are consistent
- [ ] `ASCEND_CUSTOM_OPP_PATH` points to OPP package root directory (not .so directory)
- [ ] Kernel binary / source code path doesn't depend on temporary working directory
- [ ] PyTorch scenario simultaneously check Python side and GE side deliverables are both loaded
- [ ] TensorFlow scenario simultaneously check TF side .so, GE side .so and npu_supported_ops.json (GE side no need to additionally write REG_OP)
- [ ] Offline OM scenario confirm `soc_version` matches actual environment
- [ ] ONNX scenario confirm parser plugin .so and GE deliverable .so are both loaded, and `OriginOpType` format is correct

### 8.2 Common Error Troubleshooting

| Phenomenon | Possible Cause | Troubleshooting Method |
|------|---------|---------|
| GE cannot find custom operator | `ASCEND_CUSTOM_OPP_PATH` not configured or path wrong | Check environment variable, confirm .so exists under `op_graph/lib/<os>/<arch>/` |
| Execute returns null input | Input index mismatches REG_OP definition | Verify REG_OP INPUT order and GetInputTensor index in Execute |
| Compile-time shape empty | Compile phase input shape not ready | Check whether correctly read GetInputTensor in Compile |
| OM load Execute fails | Deserialize didn't correctly restore binary | Check Serialize/Deserialize buffer format is symmetric |
| PyTorch graph mode errors | Converter not registered or op type name mismatch | Confirm GE op type in `register_fx_node_ge_converter` matches REG_AUTO_MAPPING_OP |
| TensorFlow operator not visible | npu_supported_ops.json missing or format error | Check JSON file exists under `framework/tensorflow/` |
| ONNX parse errors "The type is not supported" | ONNX parser plugin not loaded or OriginOpType mismatch | Confirm parser plugin .so loaded through `ASCEND_CUSTOM_OPP_PATH`, and `OriginOpType` format is `"domain::version::OpType"` |
