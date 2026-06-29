# Custom Operator Graph Integration Samples

This directory provides samples related to custom operator graph integration, covering different graph composition entry points, operator programming languages and model sink links.

## Sample Overview

| Sample | Scenario | Graph Composition Entry | Operator Programming Language | Operator Compilation Method | Model Sink Capability | Link |
| --- |---------| -- | --- |-----------| --- | --- |
| `ascendc_add_custom` | Ascend C operator enters graph through GE | PyTorch + TorchAir | Ascend C | CMake compilation | Not involved | [README](./ascendc_add_custom/README_en.md) |
| `triton_add_custom` | Triton operator enters graph through GE | TensorFlow | Triton | Pre-compiled as `npubin` | Not involved | [README](./triton_add_custom/README_en.md) |
| `compilable_add_custom` | Ascend C operator enters graph through GE and generates om offline model | GE + ATC offline compilation | Ascend C | RTC operator runtime compilation | Supports model sink to om offline model | [README](./compilable_add_custom/README_en.md) |
| `data_dependent_shape_custom` | Data dependent shape operator | GE | Ascend C | CMake compilation | Not involved | [README](data_dependent_shape_custom/README_en.md) |
| `args_refresh_add_custom` | ArgsUpdater address refresh + MallocReadOnlyDevArgs + performance comparison | GE online execution | Ascend C | RTC runtime compilation | Online address refresh performance comparison | [README](./args_refresh_add_custom/README_en.md) |

## General Development Process

### 1. Write Custom Operator Deliverables

Custom operator deliverables are usually a `.so` that can be loaded by GE / framework plugins, the core is implementing capability interfaces in `inc/graph_metadef/external/graph/custom_op.h`, and registering through `REG_AUTO_MAPPING_OP`.
GE native graph composition scenarios also need to provide `REG_OP` proto header file, describing operator's inputs, outputs and attributes, for use when creating op type on graph composition side.

Currently provided interface functionality:

| Interface / Macro | Purpose |
|------------------|---------|
| `class BaseCustomOp` | Common base class for custom operator capability interfaces, user implementation classes combine and inherit other capability interfaces as needed. |
| `class EagerExecuteOp` | Runtime execution capability, can get input Tensor, allocate output Tensor, allocate workspace and initiate kernel call. |
| `class ShapeInferOp` | Shape / DataType derivation capability, used to set output description during compilation or graph composition phase. |
| `class CompilableOp` | Online compilation capability, suitable for reading input metadata, compiling kernel or preparing compilation products during GE/ATC compilation process. |
| `class PortableOp` | Serialization / deserialization capability, used to serialize / deserialize custom operator kernel bin during OM save and load phases. |
| `REG_OP` | Define GE native graph composition visible operator proto, usually copied to `op_graph/include/` with deliverables. |
| `REG_AUTO_MAPPING_OP` | Static registration of custom operator type and creation macro, GE creates corresponding implementation class by operator type. |

Interface combination selection by scenario:

| Scenario | Recommended Implementation |
|---------|---------------------------|
| Dynamic graph online execution | `EagerExecuteOp` + `ShapeInferOp(optional)` |
| Dynamic graph online execution + operator online compilation | `EagerExecuteOp` + `CompilableOp` + `ShapeInferOp(optional)` |
| Static graph offline sink OM model execution + operator online compilation | `EagerExecuteOp` + `CompilableOp` + `ShapeInferOp(optional)` + `PortableOp` |
| Address refresh + online execution | `EagerExecuteOp` + `ArgsUpdater` + `ShapeInferOp` |

Deliverable naming can be chosen according to samples, but need to ensure operator type, registration class name and graph composition side used op type are aligned.

### 2. Configure Deliverable Path

Custom operator deliverables need to be exposed to GE / ATC / framework plugins through `ASCEND_CUSTOM_OPP_PATH`. Recommend organizing by OPP package root directory:

```text
<custom_opp_root>/
├── op_graph
│   ├── include
│   │   └── xxx_custom.h
│   └── lib
│       └── <os>
│           └── <arch>
│               └── libxxx_custom_op.so
└── framework
    └── tensorflow
        └── npu_supported_ops.json  // Needed when Tensorflow enters graph
```

GE graph composition configuration method:

```bash
export ASCEND_CUSTOM_OPP_PATH="<custom_opp_root>:$ASCEND_CUSTOM_OPP_PATH"
```

`<os>/<arch>` select by running environment, for example `linux/x86_64`, `linux/aarch64`. When offline saving OM, if model needs to carry custom operator so, GE will read `.so` deliverables under `<custom_opp_root>/op_graph/lib/<os>/<arch>/` based on running environment.

### 3. Graph Composition and Frontend Integration

When GE native graph composition, graph composition side needs to see `REG_OP` proto header file, and create op type consistent with `REG_AUTO_MAPPING_OP` registration name in graph. Refer to `compilable_add_custom` and `data_dependent_shape_custom`.

When PyTorch / TorchAir enters graph, besides GE side custom operator `.so`, also need Python / PyTorch side registration and conversion logic:

| Frontend | Additional Deliverables / Configuration |
| --- | --- |
| PyTorch + TorchAir | Need `TORCH_LIBRARY` / `TORCH_LIBRARY_IMPL` to register Python visible operators, and convert PyTorch nodes to GE custom operators through TorchAir converter. |
| TensorFlow | Need TensorFlow side custom operator `.so`, and provide `npu_supported_ops.json` recognizable by framework plugin; TensorFlow Adapter handles graph composition and will bring in GE side `REG_OP` information. |

Different frontends have different "enter graph" responsibilities, but ultimately need to let op type in GE graph map to custom operator implementation class.

### 4. Compile and Run

Common run methods:

| Method | Description | Interface Requirements |
| --- |---------| --- |
| Online / Direct execution | Execute directly after in-process graph composition, or execute during framework graph mode runtime. | Usually need `EagerExecuteOp`, and implement `ShapeInferOp` as needed. |
| Offline OM | Generate offline OM model through ATC after graph composition, then load and execute by ACL. | Need `PortableOp` to serialize compilation products into OM, and deserialize and restore during execution phase. |

If only framework online graph mode execution, can not implement `PortableOp`. If goal is `AIR -> ATC -> OM -> ACL` offline model link, need to consider how compilation products are saved and restored with model.

### 5. Development Checklist

- Operator type name, registration class name, graph composition side op type remain consistent.
- Kernel bin, source code origin clear, path does not depend on temporary working directory.
- `ASCEND_CUSTOM_OPP_PATH` points to OPP package root directory, not arbitrarily pointing to some `.so` directory, unless corresponding sample explicitly adopts simplified directory.
- TensorFlow / PyTorch enter graph scenarios simultaneously check whether framework side deliverables and GE side deliverables are both loaded.
