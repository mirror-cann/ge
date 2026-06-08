# GE 自定义算子开发指南

## 1. 概述

自定义算子允许开发者将自有 kernel（Ascend C、Triton 或其他设备侧实现）接入 GE 图编译和执行流程，而无需修改 GE 框架代码。

**适用场景：**

- 框架前端（PyTorch / TensorFlow）引入了 GE 不认识的算子，需要在 NPU 上执行
- 需要自定义 kernel 实现（如融合算子、特定优化）并接入 GE 编译和下沉流程
- 需要在 GE 图中嵌入第三方 kernel binary

**与 GE 内置算子的区别：**

| 维度 | 内置算子 | 自定义算子 |
|------|---------|-----------|
| 注册方式 | GE 内部 REG_OP + 引擎注册 | REG_OP + REG_AUTO_MAPPING_OP，以 .so 交付件加载 |
| 编译产物 | 随 GE 发布 | 独立 .so，通过 ASCEND_CUSTOM_OPP_PATH 加载 |
| 执行方式 | GE 引擎调度（DNNEngine 等） | 框架回调用户实现的 Execute / Compile |
| 更新方式 | 随 GE 版本更新 | 独立于 GE，可随时替换 .so |

---

## 2. 核心概念

### 2.1 接口体系

所有自定义算子实现类继承自 `BaseCustomOp`，按需组合以下能力接口：

```
BaseCustomOp（公共基类）
├── EagerExecuteOp    → Execute(ctx)          运行时执行
├── ShapeInferOp      → InferShape(ctx)       Shape 推导
│                     → InferDataType(ctx)    DataType 推导
├── CompilableOp      → Compile(ctx)          在线编译
├── PortableOp        → Serialize(buf)        序列化到 OM
│                     → Deserialize(buf)      从 OM 反序列化
└── ArgsUpdater       → UpdateHostArgs(ctx)   I/O 地址刷新
```

每个接口对应一个独立的回调时机，GE 在图编译或执行的特定阶段调用对应回调。

### 2.2 注册机制

自定义算子需要两个注册：

**REG_OP** — 定义算子的输入、输出和属性 proto，供构图侧创建节点时使用：

```cpp
REG_OP(MyCustomOp)
    .INPUT(x, TensorType({DT_FLOAT, DT_FLOAT16}))
    .INPUT(y, TensorType({DT_FLOAT, DT_FLOAT16}))
    .OUTPUT(z, TensorType({DT_FLOAT, DT_FLOAT16}))
    .OP_END_FACTORY_REG(MyCustomOp);
```

**REG_AUTO_MAPPING_OP** — 将 C++ 实现类映射到 op type，GE 按 op type 名查找并实例化：

```cpp
class MyCustomOp : public EagerExecuteOp { /* ... */ };
REG_AUTO_MAPPING_OP(MyCustomOp);
```

**命名约束**：REG_OP 中的 op type 名、REG_AUTO_MAPPING_OP 的类名、构图侧使用的 op type 必须三者一致。

### 2.3 交付件与 OPP 包

自定义算子以 `.so` 交付件形式被 GE 加载，通过 `ASCEND_CUSTOM_OPP_PATH` 环境变量指定路径。推荐的 OPP 包目录结构：

```
<custom_opp_root>/
├── op_graph/
│   ├── include/
│   │   └── my_custom_op.h          // REG_OP proto 头文件
│   └── lib/
│       └── <os>/<arch>/
│           └── libcust_opapi.so     // 交付件 .so
└── framework/                       // 框架侧交付件（按需）
    └── tensorflow/
        └── npu_supported_ops.json
```

配置方式：

```bash
export ASCEND_CUSTOM_OPP_PATH="<custom_opp_root>:$ASCEND_CUSTOM_OPP_PATH"
```

---

## 3. 快速开始

以下 5 步实现一个最小自定义算子（以 Add 为例）：

**Step 1: 定义算子 proto**

```cpp
// my_add.h
REG_OP(AddCustom)
    .INPUT(x, TensorType({DT_FLOAT, DT_FLOAT16}))
    .INPUT(y, TensorType({DT_FLOAT, DT_FLOAT16}))
    .OUTPUT(z, TensorType({DT_FLOAT, DT_FLOAT16}))
    .OP_END_FACTORY_REG(AddCustom);
```

**Step 2: 实现执行逻辑**

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

**Step 3: 注册**

```cpp
REG_AUTO_MAPPING_OP(AddCustom);
```

**Step 4: 编译为 .so**

```bash
cmake -S . -B build && cmake --build build -j$(nproc)
```

**Step 5: 配置并运行**

```bash
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/build:$ASCEND_CUSTOM_OPP_PATH"
# 运行你的构图/执行程序
```

> 完整可运行代码参见 [`examples/custom_op/ascendc_add_custom/`](../../../examples/custom_op/ascendc_add_custom/README.md)。

---

## 4. 场景选择指南

### 4.1 决策表

| 场景 | 推荐接口组合 | 典型链路 | 参考样例 |
|------|-------------|---------|---------|
| 动态图在线执行 | `EagerExecuteOp` + `ShapeInferOp`(可选) | 构图 → 直接执行 | [ascendc_add_custom](../../../examples/custom_op/ascendc_add_custom/README.md) |
| 在线编译 + 在线执行 | `EagerExecuteOp` + `CompilableOp` + `ShapeInferOp`(可选) | 构图 → Compile → 执行 | — |
| 离线 OM 模型下沉 | `EagerExecuteOp` + `CompilableOp` + `ShapeInferOp` + `PortableOp` | AIR → ATC → OM → ACL | [compilable_add_custom](../../../examples/custom_op/compilable_add_custom/README.md) |
| 数据依赖 shape | `EagerExecuteOp` + `ShapeInferOp` | 构图 → 执行 + shape buffer 回写 | [data_dependent_shape_custom](../../../examples/custom_op/data_dependent_shape_custom/README.md) |

### 4.2 场景 A：动态图在线执行

最简单的场景。kernel 已预编译好（Ascend C `.asc` 同库编译、Triton `.npubin` 等），只需在 Execute 中加载并 launch。

```
构图 → GE 回调 Execute → 获取输入 → 分配输出 → launch kernel → 返回
```

**关键点：**
- 只需实现 `EagerExecuteOp`
- ShapeInferOp 可选：如果输出 shape 与输入相同且 REG_OP 已声明，可省略
- kernel binary 随 .so 一起编译，或在 Execute 中从文件加载

### 4.3 场景 B：在线编译 + 在线执行

kernel 源码需要在 GE 编译阶段通过 RTC（Runtime Compilation）编译为 device binary。

```
构图 → GE 回调 Compile（RTC 编译 kernel）
     → GE 回调 Execute（加载 binary，launch kernel）
```

**关键点：**
- Compile 回调中可通过 `ctx->GetInputTensor(i)` 获取输入的 shape、dtype、format 等完整元信息
- 建议按 shape 生成 binary key，支持多 shape / 多 kernel 管理
- 编译产物缓存在实现类的成员变量中，供 Execute 使用

### 4.4 场景 C：离线 OM 模型下沉

在场景 B 基础上，需要将编译产物序列化到 OM 模型文件中，后续加载 OM 时反序列化恢复。

```
构图 → ATC 离线编译 → 回调 Compile（RTC 编译）
                    → 回调 Serialize（binary 写入 OM）
     → ACL 加载 OM → 回调 Deserialize（从 OM 恢复 binary）
                    → 回调 Execute（launch kernel）
```

**关键点：**
- 必须实现 `PortableOp`，Serialize/Deserialize 的 buffer 格式由用户自定义，GE 只透传不解析
- ShapeInferOp 在 OM 编译阶段被调用，用于推导输出 shape/dtype
- 支持多份 binary 的序列化（按 key 管理）

### 4.5 场景 D：数据依赖 shape（三类算子）

输出 shape 依赖运行时输入数据（如 Where、NonZero），编译期只能确定上界。

```
InferShape → 输出 shape 上界（含 unknown 维）
Execute    → 按上界分配输出 + 分配 shape buffer
           → launch kernel（kernel 写回实际 shape 到 shape buffer）
           → stream sync → D2H 读回 shape buffer → 更新输出 shape
```

**关键点：**
- InferShape 中用 `-1` 表示 unknown 维
- Execute 中按最大 shape 分配输出，额外分配 shape buffer（通过 `MallocWorkSpace`）
- kernel 执行后需自行同步 stream 并读回 shape buffer
- shape buffer 格式：`[ndim, d0, d1, ...]`（uint64_t 数组）

> 完整代码参见 [`examples/custom_op/data_dependent_shape_custom/`](../../../examples/custom_op/data_dependent_shape_custom/README.md)。

---

## 5. 接口详解

### 5.1 EagerExecuteOp::Execute

运行时执行回调，是最核心的接口。

```cpp
graphStatus Execute(gert::EagerOpExecutionContext *ctx) override;
```

**上下文 API：**

| 方法 | 用途 |
|------|------|
| `ctx->GetInputTensor(index)` | 获取输入 Tensor（含 addr、shape、dtype、format） |
| `ctx->MallocOutputTensor(index, shape, format, dtype)` | 分配输出 Tensor device 内存 |
| `ctx->MallocWorkSpace(size)` | 分配 workspace device 内存 |
| `ctx->GetStream()` | 获取执行流 |
| `ctx->GetOutputTensor(index)` | 获取已分配的输出 Tensor |
| `ctx->MakeOutputRefInput(out_idx, in_idx)` | 输出引用输入地址（零拷贝） |

**典型流程：**

```cpp
graphStatus Execute(gert::EagerOpExecutionContext *ctx) {
  auto *x = ctx->GetInputTensor(0);
  auto *y = ctx->MallocOutputTensor(0, x->GetShape(), x->GetFormat(), x->GetDataType());
  void *stream = ctx->GetStream();
  LaunchMyKernel(x->GetAddr(), y->GetAddr(), stream);
  return GRAPH_SUCCESS;
}
```

**注意事项：**
- 输出内存由 context 管理生命周期，调用者无需释放
- `GetInputTensor` 返回 `const Tensor*`，不可修改输入数据
- 对于预编译 kernel binary 场景，需在 Execute 中加载 binary 并通过 `aclrtLaunchKernelWithHostArgs` launch

### 5.2 ShapeInferOp::InferShape / InferDataType

编译期 Shape 和 DataType 推导回调。

```cpp
graphStatus InferShape(gert::InferShapeContext *ctx) override;
graphStatus InferDataType(gert::InferDataTypeContext *ctx) override;
```

**InferShapeContext API：**

| 方法 | 用途 |
|------|------|
| `ctx->GetInputShape(index)` | 获取输入 origin Shape（`const Shape*`） |
| `ctx->GetOutputShape(index)` | 获取可修改的输出 Shape（`Shape*`） |
| `ctx->GetInputTensor(index)` | 获取输入 Tensor（可访问数据） |

**InferDataTypeContext API：**

| 方法 | 用途 |
|------|------|
| `ctx->GetInputDataType(index)` | 获取输入 DataType |
| `ctx->SetOutputDataType(index, dtype)` | 设置输出 DataType |

**典型流程：**

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

在线编译回调，在 GE/ATC 编译阶段被调用。

```cpp
graphStatus Compile(gert::OpCompileContext *ctx) override;
```

**OpCompileContext API：**

| 方法 | 用途 |
|------|------|
| `ctx->GetInputTensor(index)` | 获取编译期输入 Tensor（含 shape、dtype、format） |
| `ctx->GetOutputTensor(index)` | 获取编译期输出 Tensor |
| `ctx->GetOption(key, value)` | 获取编译选项 |
| `ctx->GetPlatformInfos(...)` | 获取目标平台信息（芯片型号等） |

**典型流程：**

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

序列化/反序列化回调，用于 OM 模型下沉。

```cpp
graphStatus Serialize(std::vector<uint8_t> &buffer) override;
graphStatus Deserialize(const std::vector<uint8_t> &buffer) override;
```

**关键点：**
- buffer 格式完全由用户自定义，GE 不解析只透传
- Serialize 在 ATC 编译阶段被调用，将 Compile 产物写入 buffer
- Deserialize 在 OM 加载执行阶段被调用，从 buffer 恢复 Compile 产物
- 多 binary 场景建议序列化格式：`[count][key_len][key][bin_len][bin]...`

### 5.5 ArgsUpdater::UpdateHostArgs

I/O 地址变化时的回调，用于刷新 kernel args 中的地址引用。

```cpp
graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override;
```

**上下文 API：**

| 方法 | 用途 |
|------|------|
| `ctx->GetKernelArgs(placement, index)` | 获取指定位置的 kernel args |
| 继承自 EagerOpExecutionContext 的所有方法 | 获取输入/输出 Tensor 等 |

**适用场景：** 静态图下沉后，模型加载时 I/O 地址可能与编译时不同，需要刷新 args buffer 中的地址。

---

## 6. 前端接入

### 6.1 GE 原生构图

GE 原生构图时，构图侧需要能看到 REG_OP proto 头文件，并在图中创建与 REG_AUTO_MAPPING_OP 注册名一致的 op type。

```cpp
auto op = ge::OperatorFactory::CreateOperator("my_node", "AddCustom");
op.SetInput("x", input_x);
op.SetInput("y", input_y);
graph.AddOp(op);
```

### 6.2 PyTorch + TorchAir 入图

PyTorch 场景需要两侧交付件配合：

```
Python 侧                              GE 侧
┌─────────────────────┐               ┌──────────────────────┐
│ TORCH_LIBRARY       │               │ REG_OP + REG_AUTO_   │
│ (定义算子签名)       │   converter   │ MAPPING_OP           │
│                     │ ──────────→   │ (交付件 .so)          │
│ TORCH_LIBRARY_IMPL  │               │                      │
│ (注册 PrivateUse1   │               │                      │
│  / Meta / XLA key)  │               │                      │
└─────────────────────┘               └──────────────────────┘
```

**Python 侧需要：**
1. `TORCH_LIBRARY` 定义算子 schema
2. `TORCH_LIBRARY_IMPL` 注册 PrivateUse1（NPU 执行）、Meta（shape 推导）、XLA（图编译）等 key
3. `@torchair.register_fx_node_ge_converter` 将 PyTorch FX 节点映射到 GE 自定义算子

```python
@torchair.register_fx_node_ge_converter(torch.ops.my_ops.my_op.default)
def convert_my_op(x, y, meta_outputs=None):
    return torchair.ge.custom_op("AddCustom", inputs={"x": x, "y": y}, outputs=["z"])
```

> 完整代码参见 [`examples/custom_op/ascendc_add_custom/`](../../../examples/custom_op/ascendc_add_custom/README.md)。

### 6.3 TensorFlow 入图

TensorFlow 场景需要：
1. TensorFlow 侧自定义算子 `.so`（声明算子可见）
2. GE 侧交付件 `.so`（只需 `REG_AUTO_MAPPING_OP`，无需额外提供 `REG_OP`）
3. `npu_supported_ops.json`（供 TensorFlow Adapter 识别）

```
TensorFlow 侧                    GE 侧
┌──────────────────┐            ┌────────────────────────────┐
│ libcustom_ops.so │            │ libcust_opapi.so           │
│ (算子声明)        │  Adapter   │ (REG_AUTO_MAPPING_OP)      │
│                  │ ────────→  │                            │
│ npu_supported_   │            │ GE proto 由 TF 原型自动生成  │
│ ops.json         │            │ 无需额外编写 REG_OP         │
└──────────────────┘            └────────────────────────────┘
```

**与 GE 原生构图的区别**：TensorFlow 场景下，`REG_AUTO_MAPPING_OP` 可以从 TF 算子原型自动生成 GE 算子原型（输入、输出、属性），开发者无需额外编写 `REG_OP` 定义。这进一步降低了 TensorFlow 自定义算子的接入负担。

> 完整代码参见 [`examples/custom_op/triton_add_custom/`](../../../examples/custom_op/triton_add_custom/README.md)。

### 6.4 ONNX 入图

ONNX 前端与 PyTorch / TensorFlow 不同：GE 的 ONNX Parser 不识别的 op type 会直接报错（`PARAM_INVALID`），因此需要额外编写一个 **ONNX 解析插件**，将 ONNX 节点的属性映射为 GE 算子属性。

```
ONNX 模型                         ONNX 解析插件                    GE 侧
┌──────────────────┐            ┌────────────────────────┐       ┌──────────────────────┐
│ NodeProto        │            │ REGISTER_CUSTOM_OP     │       │ REG_OP + REG_AUTO_   │
│ (op_type,        │  dlopen    │ (OriginOpType 映射     │       │ MAPPING_OP           │
│  attributes)     │ ────────→  │  ParseParamsFn 解析)   │ ───→  │ (交付件 .so)          │
└──────────────────┘            └────────────────────────┘       └──────────────────────┘
```

**ONNX 算子类型标识格式：** `"domain::version::OpType"`（如 `"ai.onnx::11::Conv"`、`"com.example::1::MyOp"`）。Parser 会按此格式在 OpRegistry 中查找映射，找不到则解析失败。

**ONNX 解析插件需要：**

1. 使用 `REGISTER_CUSTOM_OP` 注册 ONNX op type 到 GE op type 的映射
2. 实现 `ParseParamsFn`，从 ONNX `NodeProto` 中提取属性并设置到 GE `Operator`

```cpp
#include "register/register.h"
#include "proto/onnx/ge_onnx.pb.h"

// 属性解析函数：将 ONNX NodeProto 属性映射到 GE Operator 属性
static domi::Status ParseMyOp(const google::protobuf::Message *op_src, ge::Operator &op_dest) {
  auto *node = dynamic_cast<const ge::onnx::NodeProto *>(op_src);
  for (const auto &attr : node->attribute()) {
    if (attr.name() == "alpha") {
      op_dest.SetAttr("alpha", attr.f());
    }
  }
  return domi::SUCCESS;
}

// 注册：将 ONNX op type 映射到 GE op type
REGISTER_CUSTOM_OP("MyCustomOp")
    .FrameworkType(domi::ONNX)
    .OriginOpType({"ai.onnx::11::MyOp", "ai.onnx::13::MyOp"})
    .ParseParamsFn(ParseMyOp);
```

**REGISTER_CUSTOM_OP 关键方法：**

| 方法 | 用途 |
|------|------|
| `.FrameworkType(domi::ONNX)` | 声明为 ONNX 框架插件 |
| `.OriginOpType("domain::ver::Type")` | 指定 ONNX op type（支持列表，覆盖多版本） |
| `.ParseParamsFn(fn)` | 属性解析函数（`Message* → Operator&`） |
| `.ParseParamsByOperatorFn(fn)` | 自动映射路径的属性解析（`Operator → Operator`） |
| `.ParseOpToGraphFn(fn)` | 将单个算子展开为子图（可选） |
| `.ParseSubgraphPostFn(fn)` | 含子图算子的后处理（可选） |

**交付件组织：** ONNX 场景需要两个 .so：

| 交付件 | 职责 | 放置路径 |
|--------|------|---------|
| ONNX 解析插件 .so | `REGISTER_CUSTOM_OP` + `ParseParamsFn`，负责 ONNX → GE 的属性映射 | `ASCEND_CUSTOM_OPP_PATH` 下的插件目录 |
| GE 自定义算子 .so | `REG_OP` + `REG_AUTO_MAPPING_OP` + 执行逻辑 | `op_graph/lib/<os>/<arch>/` |

**注意事项：**
- ONNX 解析插件 .so 必须在 ATC 编译前通过 `ASCEND_CUSTOM_OPP_PATH` 加载，否则 Parser 无法识别自定义 op type
- `OriginOpType` 中的 domain 为空时默认使用 `"ai.onnx"`，自定义 domain 需显式指定
- 如果 ONNX 算子可映射为已有 GE 算子的组合，可通过 `ParseOpToGraphFn` 将其展开为子图，无需编写 GE 侧自定义算子 .so

---

## 7. 构建与部署

### 7.1 CMake 构建配置

关键构建要素：

```cmake
# 头文件路径（必须包含）
target_include_directories(my_op PRIVATE
    ${ASCEND_HOME_PATH}/include
    ${ASCEND_HOME_PATH}/include/graph
    ${ASCEND_HOME_PATH}/include/register
    ${ASCEND_HOME_PATH}/include/external
)

# 链接库（按需选择）
target_link_libraries(my_op PRIVATE
    ascendcl acl_rt ge_compiler ge_executor
)

# 输出 .so
add_library(my_op SHARED custom_op.cpp)
```

**PyTorch 场景额外依赖：** 需链接 `torch_npu`、`c10`、`lowering` 等，并包含 torch_npu 头文件路径。

**Ascend C kernel 同库编译：** 使用 `find_package(ASC REQUIRED)` 并将 `.asc` 文件加入 `add_library` 源文件列表。

### 7.2 OPP 包目录结构

构建产物需按 OPP 包规范安装：

```
output/                              # ASCEND_CUSTOM_OPP_PATH 指向此目录
├── op_graph/
│   ├── include/my_op.h              # proto 头文件（供构图侧使用）
│   └── lib/linux/x86_64/
│       ├── libcust_opapi.so         # 交付件
│       └── my_kernel.cpp            # kernel 源码（RTC 场景需要）
└── framework/tensorflow/
    └── npu_supported_ops.json       # TensorFlow 场景需要
```

### 7.3 ATC 离线编译流程

离线 OM 下沉的完整链路：

```bash
# 1. 构图并导出 AIR
./graph_build_program  # 生成 model.air

# 2. ATC 离线编译
atc --model=model.air --framework=1 --output=model --soc_version=Ascend910B1

# 3. ACL 加载执行
./model_exec_program model.om
```

**前提：** `ASCEND_CUSTOM_OPP_PATH` 已指向包含交付件的 OPP 包根目录。ATC 编译时会自动加载 .so 并回调 Compile 和 Serialize。

---

## 8. 调试与常见问题

### 8.1 开发检查项

- [ ] 算子类型名、注册类名、构图侧 op type 三者一致
- [ ] `ASCEND_CUSTOM_OPP_PATH` 指向 OPP 包根目录（而非 .so 所在目录）
- [ ] kernel binary / 源码路径不依赖临时工作目录
- [ ] PyTorch 场景同时检查 Python 侧和 GE 侧交付件是否都已加载
- [ ] TensorFlow 场景同时检查 TF 侧 .so、GE 侧 .so 和 npu_supported_ops.json（GE 侧无需额外编写 REG_OP）
- [ ] 离线 OM 场景确认 `soc_version` 与实际环境一致
- [ ] ONNX 场景确认解析插件 .so 和 GE 交付件 .so 都已加载，且 `OriginOpType` 格式正确

### 8.2 常见错误排查

| 现象 | 可能原因 | 排查方法 |
|------|---------|---------|
| GE 找不到自定义算子 | `ASCEND_CUSTOM_OPP_PATH` 未配置或路径错误 | 检查环境变量，确认 `op_graph/lib/<os>/<arch>/` 下有 .so |
| Execute 返回 null input | 输入索引与 REG_OP 定义不匹配 | 核对 REG_OP 的 INPUT 顺序和 Execute 中的 GetInputTensor 索引 |
| 编译期 shape 为空 | Compile 阶段输入 shape 未就绪 | 检查是否在 Compile 中正确读取 GetInputTensor |
| OM 加载后 Execute 失败 | Deserialize 未正确恢复 binary | 检查 Serialize/Deserialize 的 buffer 格式是否对称 |
| PyTorch 图模式报错 | converter 未注册或 op type 名不匹配 | 确认 `register_fx_node_ge_converter` 中的 GE op type 与 REG_AUTO_MAPPING_OP 一致 |
| TensorFlow 算子不可见 | npu_supported_ops.json 缺失或格式错误 | 检查 JSON 文件是否在 `framework/tensorflow/` 下 |
| ONNX 解析报 "The type is not supported" | ONNX 解析插件未加载或 OriginOpType 不匹配 | 确认解析插件 .so 已通过 `ASCEND_CUSTOM_OPP_PATH` 加载，且 `OriginOpType` 格式为 `"domain::version::OpType"` |
