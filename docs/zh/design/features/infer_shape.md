# GE InferShape 特性分析

## 1 特性背景

### 1.1 问题定义

在深度学习图编译器中，Shape 推导（InferShape）是一项基础能力：给定算子的输入 Shape、属性和可能的输入数据值，确定算子输出张量的维度信息（Shape）和数据类型（DataType）。

Shape 推导在多个阶段被依赖：

- **编译阶段**：格式推导（InferFormat）、内存规划（根据 Shape 计算tensor大小）、常量折叠
- **执行阶段**：动态 Shape 场景下，运行时需要根据实际输入 Shape 重新推导输出 Shape，以分配正确大小的内存
- **离线分析**：通过 atc 工具 dump 出模型的 Shape 信息，用于调试和验证

GE 作为面向昇腾 AI 处理器的图编译器，Shape 推导面临额外的复杂性：昇腾硬件对数据排布格式（如 NC1HWC0、FZ 等）有亲和性偏好，同一个张量在用户视角和硬件执行视角下拥有不同的维度描述。这要求 InferShape 机制必须同时维护两套 Shape 信息——OriginShape 和 StorageShape，并在不同阶段正确使用。

### 1.2 设计目标

GE 的 InferShape 机制围绕以下目标设计：

1. **语义正确**：准确反映算子的数学语义，输出 Shape 必须与算子计算结果一致
2. **格式感知**：区分用户语义层面的 Shape（OriginShape）和实际存储层面的 Shape（StorageShape），两者独立管理
3. **动态 Shape 支持**：支持运行时 Shape 推导，处理输入维度未知（-1）、维度范围、符号化表达式等场景
4. **编译-运行一致**：编译期和运行期使用相同的算子 InferShape 注册函数，保证推导结果一致

## 2 OriginShape 与 StorageShape

### 2.1 为什么需要两套 Shape

GE 内部对张量维度信息的描述分为两套体系：**Origin** 和 **Storage**。这不是冗余设计，而是昇腾硬件架构的必然要求。

**Origin** 描述用户语义层面的信息：

- `OriginFormat`：用户定义的数据排布格式，如 NCHW
- `OriginShape`：用户理解的维度信息，如 `[8, 3, 224, 224]`

Origin 由前端框架或用户显式给出，直接反映用户意图，不包含任何硬件实现假设。在整个编译过程中，Origin 始终保留，作为语义基准——任何格式优化都必须在不破坏 Origin 语义的前提下进行。

**Storage** 描述实际执行时的信息：

- `StorageFormat`：内存中的实际布局，如 NC1HWC0（将 C 轴拆分为 C1 和 C0）
- `StorageShape`：内存中的实际维度，如 `[8, 1, 224, 224, 16]`

Storage 由 GE 编译器根据算子能力、格式亲和性、全图数据流等因素推导得到，是一种面向执行性能的工程选择。

两者的关系可以通过下表直观理解：

| 视角    | 接口                 | 示例内容                                         | 说明                         |
| ------- | -------------------- | ------------------------------------------------ | ---------------------------- |
| Origin  | `GetOriginFormat()`  | NCHW                                             | 用户语义上的格式             |
| Origin  | `GetOriginShape()`   | [8, 3, 224, 224]                                 | 用户理解的 Shape             |
| Storage | `GetStorageFormat()` | NC1HWC0                                          | 实际执行使用的格式           |
| Storage | `GetStorageShape()`  | [8, 1, 224, 224, 16]                             | 执行阶段的内存形态（含补维） |

核心差异在于：NCHW 格式下 C 维度为 3，但 NC1HWC0 格式要求 C0 维度为 16（硬件对齐），因此 C1 = ceil(3/16) = 1。这就导致 StorageShape 从 4 维变成了 5 维，且数值完全不同。

### 2.2 推导时机的差异

**OriginShape 通过 InferShape 过程推导**。它以计算图输入 Shape 为起点，按照算子语义自前向后逐层推导，直至图的输出。这一过程是图编译器的标准做法。

**StorageShape 不是独立推导的结果**。当 OriginShape、OriginFormat 与 StorageFormat 均已确定后，StorageShape 根据 StorageFormat 对应的内存排布方式计算得到。具体来说，编译期在 `graph_prepare.cc` 的 `TransferStorageShapeAccordingFormat()` 中分两步完成：

1. **维度扩展（ExpandDimension）**：根据 origin_format 到 storage_format 的 reshape_type，将 origin_shape 扩展为对应维度的中间 shape
2. **格式转换（TransferShape）**：根据格式语义（如 NC1HWC0 的 C 轴拆分规则），将中间 shape 转换为最终的 storage_shape

同样的逻辑也存在于运行时的 `TransformOutputShape()` 中：算子 InferShape 函数先写入 OriginShape，再由框架自动完成到 StorageShape 的转换。

### 2.3 分别在什么时候使用

| 使用场景 | 使用的 Shape | 原因 |
|----------|-------------|------|
| 算子 InferShape 函数内部 | OriginShape | InferShape 按算子数学语义推导，与内存布局无关 |
| 格式推导（InferFormat） | OriginShape | 确定用户的语义格式 |
| 内存分配大小计算 | StorageShape | 实际占用的内存大小由存储形态决定 |
| 执行引擎数据搬运 | StorageShape | 需要知道内存中的真实排布 |
| 对用户返回 Shape 信息 | OriginShape | 用户关心的是语义维度 |
| 常量折叠 | OriginShape | 折叠逻辑按数学语义执行 |
| Tiling 计算 | StorageShape | 硬件分块策略基于实际存储布局 |
| atc dump infershape json | OriginShape | 面向用户的离线分析工具 |

### 2.4 API 层面的统一封装

在 GE 的类型系统中，`Shape` 是纯数据结构，不绑定语义——它可以承载 OriginShape 也可以承载 StorageShape，取决于由哪个接口返回。

而 `StorageShape` 类（定义于 `exe_graph/runtime/storage_shape.h`）虽然名字容易引起混淆，但它实际上是**同时携带 Origin 和 Storage 的复合描述体**：

```
class StorageShape {
    Shape origin_shape_;      // OriginShape 信息
    Shape storage_shape_;     // StorageShape 信息
public:
    const Shape &GetOriginShape() const;
    const Shape &GetStorageShape() const;
    Shape *MutableStorageShape();  // 可写的 storage shape，用于格式转换
};
```

之所以将两者绑定在同一个类中，是因为仅凭 StorageShape 本身无法唯一还原语义。例如看到 `[8, 1, 224, 224, 16]` 时，C 维度原始值可能是 1 到 16 之间的任意值，OriginFormat 也可能不同。只有同时携带 Origin 和 Storage 信息，才能形成可解释的完整描述。

## 3 用户使用场景

### 3.1 单算子 Shape 推导（不再演进）

用户在调用 `aclopExecuteV2` 执行单算子时，如果算子支持动态 Shape，可能无法提前知道输出 Shape。此时可以调用 `aclopInferShape` 接口获取输出 Shape，再分配输出内存：

```
aclopInferShape(opType, numInputs, inputDesc, inputs, numOutputs, outputDesc, attr)
→ outputDesc 被原地更新为推导后的 Shape、DataType、Format、ShapeRange
→ 用户根据 outputDesc 分配输出内存
→ 调用 aclopExecuteV2 执行算子
```

### 3.2 图编译过程中的自动推导（离线编译/在线图模式）

用户构建 `ge::Graph` 并通过 `aclgrphBuildModel` 编译模型时，GE 编译器在图准备阶段自动运行 InferShape Pass，为所有算子推导输出 Shape。用户无需手动调用 InferShape。

优化级别 O1 下，GE 关闭所有图融合和 UB 融合 Pass，但保留 InferShape、常量折叠、死边消除等基础优化。

### 3.3 离线 Shape 分析（atc dump）

通过 atc 工具的 `--dump_mode=1` 参数，可以解析模型并运行 Shape 推导，将结果序列化为 JSON 文件，用于调试和分析：

```
atc --model=model.onnx --dump_mode=1 --json=infershape.json
```

### 3.4 动态 Shape 场景下的运行时推导

在动态 Shape 模型中，编译期只能确定 Shape 的范围（ShapeRange），具体的 Shape 值在运行时根据实际输入确定。运行时引擎在执行每个算子前，通过执行图中的 InferShape 节点完成实时 Shape 推导。

## 4 对外接口

### 4.1 C API：aclopInferShape

**头文件**：`acl/acl_op.h`

**函数原型**：

```c
aclError aclopInferShape(const char *opType,
                         int numInputs,
                         aclTensorDesc *inputDesc[],
                         aclDataBuffer *inputs[],
                         int numOutputs,
                         aclTensorDesc *outputDesc[],
                         aclopAttr *attr);
```

**功能**：根据算子的输入 Shape 和输入值推导输出 Shape。

**推导结果分三种情况**：
- 能得到准确输出 Shape → 返回准确值
- 无法得到准确 Shape 但能得到范围 → 动态维度记为 -1，通过 `aclGetTensorDescDimRange` 获取范围
- 无法得到 Shape 和范围（预留）→ 动态维度记为 -2

**约束**：如果算子有 DYNAMIC_INPUT 或 DYNAMIC_OUTPUT，需先调用 `aclSetTensorDescName` 设置输入输出名称，且名称需与算子 IR 原型中定义的名称一致。

### 4.2 C++ API：Operator::InferShapeAndType

**头文件**：`graph/operator.h`

**函数原型**：

```c++
graphStatus InferShapeAndType();
```

**功能**：推导 Operator 的输出 Shape 和 DataType。内部通过 OpDesc 查找算子注册的 InferShape 函数并调用。

### 4.3 算子开发接口：InferShapeContext

**头文件**：`exe_graph/runtime/infer_shape_context.h`

算子开发者通过 `InferShapeContext` 实现 Shape 推导。该类继承自 `ExtendedKernelContext`，提供以下关键接口：

| 接口 | 说明 |
|------|------|
| `GetInputShape(index)` | 获取第 index 个输入的 Shape 指针（只读） |
| `GetOutputShape(index)` | 获取第 index 个输出的 Shape 指针（可写） |
| `GetInputTensor(index)` | 获取输入 Tensor 数据（用于数据依赖的 Shape 推导） |
| `GetComputeNodeInfo()` | 获取算子元信息（类型、名称、I/O 描述） |
| `GetAttrs()` | 获取算子属性 |

算子注册方式：

```cpp
// 方式一：使用 InferShapeContext（推荐，v2 接口）
ge::graphStatus InferShapeForCast(InferShapeContext *context) {
    const gert::Shape *shape = context->GetInputShape(0);
    gert::Shape *output_shape = context->GetOutputShape(0);
    *output_shape = *shape;
    return ge::GRAPH_SUCCESS;
}
IMPL_OP(Cast).InferShape(InferShapeForCast);
```

### 4.4 相关辅助接口

| 接口 | 文件 | 说明 |
|------|------|------|
| `aclSetTensorOriginShape` | `acl/acl_op.h` | 设置 tensor 描述的原始 Shape |
| `aclSetTensorStorageShape`（废弃） | `acl/acl_op.h` | 设置 tensor 的存储 Shape |
| `CtInferShapeContext` | `ct_infer_shape_context.h` | 编译期 InferShape 上下文，扩展了 `GetInferenceContext()` 用于资源类算子 |
| `CtInferShapeRangeContext` | `ct_infer_shape_range_context.h` | 编译期 ShapeRange 推导上下文 |
| `InferShapeFuncRegister` | 算子原型库内部 | 算子 InferShape 函数注册接口 |
| `IMPLEMT_COMMON_INFERFUNC` | 通用 InferShape 宏 | 入参为 Operator 基类，支持多算子共用 |
| `BROADCAST_INFER` | 广播类 InferShape 宏 | 基于 2 个输入 Shape 的广播推导 |
| `ELMTWISE_INFER_SHAPEANDTYPE` | 逐元素算子宏 | 输入 Shape = 输出 Shape |

### 4.5 InferShapeRange 接口

动态 Shape 场景下，除了推导精确 Shape，还需要推导 Shape 的取值范围。三类算子必须实现 InferShapeRange；一类、二类算子在满足单调性条件时可使用框架自动推导。

```cpp
ge::graphStatus InferShapeRangeForWhere(InferShapeRangeContext *context) {
    // 设置每个维度可能的最小值和最大值
    context->SetOutputDimRange(0, 0, {0, x_shape_size});
    context->SetOutputDimRange(0, 1, {Rank(x), Rank(x)});
    return ge::GRAPH_SUCCESS;
}
```
- 注：一二三类算子的定义可以参考[op_impl_dev_guide.md](../../user_guides/custom_op/custom_op_v1/op_impl_dev_guide.md)

## 5 编译期实现

### 5.1 整体流程

InferShape 在编译期作为图准备（GraphPrepare）流水线的一个阶段执行。核心调用链为：

```
GraphPrepare::PrepareDynShape()
  └─ FormatAndShapeProcess()
       ├─ InferOriginFormat()            // 第一轮格式推导
       └─ InferShapeForPreprocess()      // InferShape 核心
            └─ GEPass(names_to_passes)   // BFS 拓扑遍历
                 └─ InferShapePass::Run(node)
```

### 5.2 InferShapePass 类层次

```
BaseNodePass                     // 所有节点级 Pass 的基类，提供 re-pass、suspend/resume 机制
  └─ InferBasePass               // Shape 推导骨架，协调 infer + update + peer 传播
       └─ InferShapePass          // 具体实现，注册为 O1 级别 Pass
```

`InferShapePass` 通过 `REG_PASS_OPTION("InferShapePass").LEVELS(OoLevel::kO1)` 注册，在 O1 优化级别下执行。

### 5.3 Pass 编排策略

InferShapePass 并非独立运行，而是与多个相关 Pass 组成 pass chain，在每个节点上依次执行：

| Pass | 作用 |
|------|------|
| AssertPass | 处理 Assert 节点的推断 |
| SwitchDeadBranchElimination | 消除 Switch 的死分支 |
| CondRemovePass | 移除冗余条件节点 |
| MergePass | 处理 Merge 节点的形状合并 |
| **InferShapePass** | **核心 Shape 推导** |
| ReplaceWithEmptyConstPass | 替换为空常量 |
| SplitShapeNPass | 拆分 ShapeN 节点 |
| DimensionComputePass | 维度计算 |
| ConstantClipPass | 常量裁剪 |
| ConstantFoldingPass | 常量折叠 |
| InferValueRangePass | 值范围推导 |

这种交错编排是关键设计：常量折叠可能产生新的常量值，触发下游节点需要重新推导 Shape；re-pass 机制负责自动处理这种级联更新。

### 5.4 节点遍历与 Re-Pass 机制

GEPass 采用 BFS 拓扑遍历。从入度为 0 的节点开始，处理完一个节点后，检查其所有后继节点是否已就绪（所有前驱都已处理且未被挂起），将就绪的节点加入队列。

**Re-Pass 机制**分三个层级：

| 类型 | 触发条件 | 行为 |
|------|---------|------|
| 立即重推（Immediate） | peer 节点的 TensorDesc 发生变化 | 加入队列前端，立即重新执行 |
| 延迟重推（Deferred） | 算子 InferShape 函数返回 `GRAPH_NODE_NEED_REPASS` | 本轮 BFS 结束后重新处理 |
| 全局重推（Global） | 跨子图资源变化导致其他图需要重建 | 标记关联图需要重建 |

### 5.5 子图 Shape 传播

对于包含子图的节点（如 IF、CASE、WHILE），Shape 传播分三个阶段：

1. **子图前（Before Subgraph）**：将父节点的输入 TensorDesc 传播到子图的 Data 节点
2. **子图内部**：递归执行所有 Pass
3. **子图后（After Subgraph）**：收集各子图 NetOutput 的 TensorDesc，合并为父节点的输出

合并策略：
- 标准分支合并：Shape 相同则取其一，不同维度设为 UNKNOWN_DIM，不同秩设为 UNKNOWN_RANK
- 多批次场景：取各子图中最大的 Shape
- 子图多维场景：计算 ShapeRange，差异维度标为 UNKNOWN_DIM

### 5.6 V1 控制流处理

V1 控制流（SWITCH/MERGE/LOOPCOND/ENTER/EXIT/NEXTITERATION）需要特殊处理：

- While 循环的 Exit 节点在正常遍历中被**挂起（Suspend）**，避免在循环体 Shape 尚未完全推导时过早传播
- 当遍历队列耗尽后，通过 `OnSuspendNodesLeaked()` 逐个恢复（Resume）Exit 节点，确保在循环体推导完成后再处理

### 5.7 资源上下文感知

资源类算子（创建/使用资源的算子）通过 `ResourceContextMgr` 和 `InferenceContext` 实现跨节点的资源依赖追踪：

1. 算子 InferShape 时声明依赖的资源 key（`RegisterReliedOnResourceKey`）
2. 算子修改资源 Shape 时标记变更（`AddChangedResourceKey`）
3. 变更触发所有依赖该资源的节点重新推导
4. 跨图资源变更通过 `GraphRebuildStateCtrl` 标记关联图需要重建

### 5.8 OriginShape 到 StorageShape 的转换

InferShapePass 只推导 OriginShape。StorageShape 在后续阶段根据 OriginShape、OriginFormat 和 StorageFormat 计算，分两步：

1. **维度扩展**：根据 reshape_type 将 OriginShape 扩展为中间 Shape
2. **格式转换**：根据格式语义（如 NC1HWC0 的 C 轴对齐规则）计算最终 StorageShape

转换函数 `TransferStorageShapeAccordingFormat()` 位于 `compiler/graph/preprocess/graph_prepare.cc`。

## 6 运行期实现

### 6.1 执行图中的 InferShape 节点

在动态 Shape 场景下，运行时需要在执行算子前推导输出 Shape。GE 通过 lowering 阶段在执行图中插入 InferShape 节点来实现这一能力。

#### 四种推导策略

运行时支持四种 InferShape 策略，按优先级依次尝试：

```
InferStorageShape()  分发入口
  ├─ 1. SymbolInferShape        // 符号化推导（autofuse 场景）
  ├─ 2. Regular InferShape      // 标准 v2 推导
  ├─ 3. InferShapeByRule        // 规则推导（JSON/编译二进制）
  └─ 4. CompatibleInferShape    // v1 兼容推导
```

| 策略 | 适用场景 | 执行图结构 |
|------|---------|-----------|
| Regular InferShape | 算子注册了 v2 infer_shape 函数 | `InferShape(all_shapes, FindInferShapeFunc(node_type, space_registry))` |
| CompatibleInferShape | 算子仅有 v1 InferShapeFunc | `CompatibleInferShape(CreateOpFromBuffer, FindCompatibleInferShapeFunc(node_type), shapes)` |
| SymbolInferShape | autofuse 节点（AscBackend 等） | `InferShape(symbol_shapes, infer_shape_func)` |
| InferShapeByRule | 算子附带了 Shape 推导规则 | `InferShapeByRule(LoadShapeRule(binary))` |

#### 执行流程

以 Regular InferShape 为例，运行时执行流程：

1. `FindInferShapeFunc` 节点从 `OpImplSpaceRegistryV2` 查找算子的 infer_shape 函数指针
2. `InferShape` 节点将函数指针和所有输入 StorageShape 作为输入，调用算子的 infer_shape 函数
3. 算子 InferShape 函数通过 `InferShapeContext` 接口读取输入 Shape、写入输出 OriginShape
4. `TransformAllOutputsShape()` 自动将输出 OriginShape 转换为 StorageShape（维度扩展 + 格式转换）

`FindInferShapeFunc` 仅服务于 `OpImplSpaceRegistryV2` 路径。lowering 阶段只有在 `IsInferShapeRegistered()` 已确认当前 op type 存在 v2 infer_shape 时才会构造该节点，因此运行期再次查找失败代表 registry/type/version 前后不一致，应直接失败。自定义算子的 ShapeInferOp 不通过该节点回退到进程级 `CustomOpFactory`，而是走 `LoweringCustomNode -> InferCustomOpShape -> FindCustomOp -> InferCustomOpShapeFromInput`，并使用 `GeRootModel` 注入到 `LoweringGlobalData` 的模型级 `CustomOpRegistry`。

### 6.2 执行图优化

#### FindInferShapeFunc 去重

同一类型的多个算子不需要重复创建 `FindInferShapeFunc` 节点。通过 `LoweringGlobalData` 的 `GetOrCreateUniqueValueHolder()` 方法，相同 optype 的算子共享同一个函数查找节点，减少执行图中的 Const 节点数量：

- 未优化：每个算子节点产生 `Const(op_type) + FindInferShapeFunc`，N 个同类型算子产生 2N 个节点
- 优化后：N 个同类型算子共享 1 个 `FindInferShapeFunc` 节点

#### TrustOutTensor 优化

当 `trust_shape_on_out_tensor` 选项启用时，`TrustOutTensor` Pass 消除冗余的 InferShape 节点：

- 如果 InferShape 节点的所有输出都连接到 OutputData 节点（即 Shape 信息可从模型输出直接获得），则该 InferShape 节点可被删除
- OutputData 节点直接连接到 InferShape 的上游节点，绕过 Shape 推导

#### 剪枝（Pruner）

执行图优化阶段，Pruner 会移除无用的 InferShape 相关节点：
- `FindInferShapeFunc`、`FindInferShapeRangeFunc`、`FindCompatibleInferShapeFunc` 属于 init 节点，无下游消费者时可剪枝
- `InferShape`、`CompatibleInferShape` 等属于白名单节点，无输出边时可剪枝

### 6.3 推导后的 OriginShape → StorageShape 转换

运行时的 `TransformOutputShape()` 完成与编译期相同的转换逻辑：

```
1. ExpandDimsType：将 OriginShape 扩展为与 StorageFormat 匹配的维度数
2. ShapeTransferAccordingToFormat：根据格式语义转换为 StorageShape
```

例如，OriginShape `[8, 3, 224, 224]` + OriginFormat NCHW + StorageFormat NC1HWC0 → StorageShape `[8, 1, 224, 224, 16]`。

## 7 符号化 Shape 推导

### 7.1 问题与动机

在动态 Shape 场景中，编译期无法获得具体的 Shape 值（如 batch_size 在运行时才确定）。传统的 InferShape 基于具体整数维度工作，无法处理这种情况。

符号化 Shape 推导（Symbolic InferShape）通过引入符号变量（如 `s0`, `s1`）和符号表达式（如 `s0 * s1 / 2`），在编译期完成 Shape 的结构化推导。运行时只需将实际维度值代入符号表达式，即可得到具体 Shape，无需重新执行算子的 InferShape 函数。

### 7.2 核心类型

| 类型 | 定义位置 | 说明 |
|------|---------|------|
| `ge::Expression` / `ge::Symbol` | `graph/symbolizer/symbolic.h` | 符号表达式，支持常量、变量、算术运算 |
| `gert::SymbolShape` | `exe_graph/runtime/symbolic_shape.h` | 由 `Expression` 向量组成的符号化 Shape |
| `gert::SymbolTensor` | `exe_graph/runtime/symbolic_tensor.h` | 符号化 Tensor（Shape + 符号化数据值） |
| `InferSymbolShapeContext` | `exe_graph/runtime/infer_symbol_shape_context.h` | 符号化推导上下文 |

### 7.3 推导流程

```
SymbolicInfoPreProcessor       // 预处理：消除控制流、折叠常量
  ↓
SymbolicShapeSymbolizer::Symbolize  // 将 Data 节点的 Shape 符号化
  ↓                                  // 固定维度 → 常量符号
  ↓                                  // 动态维度（-1）→ 变量符号 + Source
SymbolicShapeInference::Infer       // 拓扑遍历，逐节点调用符号化推导函数
  ↓
SymbolicShapeInference::Simplify    // 简化所有符号表达式
  ↓
SymbolicInfoPostProcessor           // 后处理：标记 merge key、符号计数、生成 guard 函数
```

### 7.4 算子实现示例

符号化推导使用 `IMPL_OP_INFER_SYMBOL_SHAPE_INNER` 宏注册，实现函数接收 `InferSymbolShapeContext`：

- **Slice**：输出维度 = `size[i]` 或 `input_shape[i] - offsets[i]`，offsets 和 size 作为符号化数据值读取
- **LayerNorm**：输出 0 与输入同 Shape；输出 1、2 在 begin_norm_axis 之后维度置为 Symbol(1)
- **Pack**：在 axis 位置插入新维度 Symbol(num)
- **Reshape**：目标 Shape 从输入 tensor 的符号化值中读取，维度为 -1 时通过 `in_size / out_size` 计算表达式

### 7.5 与常规 InferShape 的对比

| 方面 | 常规 InferShape | 符号化 InferShape |
|------|----------------|-------------------|
| 维度值 | 具体整数 | 符号表达式 |
| 推导时机 | 编译期 / 运行期 | 编译期（autofuse 阶段） |
| 动态 Shape | 需运行时重新推导 | 编译期完成结构化推导 |
| 数据值访问 | 具体数据 | 符号化数据值 |
| 上下文 | InferShapeContext | InferSymbolShapeContext |

### 7.6 Merge Key 优化

`MarkInferShapeMergeKey()` 为每个 autofuse 节点生成基于输出符号化 Shape 的确定性 key（格式如 `[dim1_dim2][dim3_dim4]`）。lowering 时，相同 key 的节点可以共享一个 InferShape 节点，减少运行时开销。

## 8 aclopInferShape 实现机制

### 8.1 端到端流程

`aclopInferShape` 是 ACL 对外暴露的 C API，内部通过以下步骤完成单算子 Shape 推导：

```
aclopInferShape(opType, numInputs, inputDesc, inputs, numOutputs, outputDesc, attr)
  │
  ├─ 参数校验（opType、输入输出数组非空）
  ├─ 延迟加载算子原型库（从 ASCEND_OPP_PATH 路径）
  ├─ 通过 OperatorFactory::CreateOperator 创建算子对象
  │    └─ 降级路径：工厂注册输入数不足时，手动构造 OpDesc
  ├─ 设置算子属性（从 aclopAttr 写入 OpDesc）
  ├─ 为每个输入构造 Const 算子
  │    ├─ 创建 TensorDesc（shape/format/dtype）
  │    ├─ 拷贝输入数据，设为 ATTR_NAME_WEIGHTS
  │    ├─ 对 Const 算子调用 InferShapeAndType()
  │    └─ 连接到目标算子的对应输入
  ├─ 对目标算子调用 InferShapeAndType()
  │    └─ 派发到算子注册的 InferShape 函数
  ├─ 回写结果到 outputDesc
  │    ├─ 从 inferOp.GetOutputDesc(i) 提取 shape/dtype/format/range
  │    └─ 原地更新 outputDesc[i] 的 dims、shapeRange、dataType、format、name
  └─ 清理：断开 Const 算子连接，释放临时数据
```

关键设计点：`aclopInferShape` 不走 OpExecutor 执行流水线，而是直接构造 `ge::Operator` 对象并调用 `InferShapeAndType()`。实际的 Shape 推导逻辑存在于算子原型库（`.so` 文件）中，GE 只是调度者。

### 8.2 Dump InferShape JSON

atc 工具通过 `--dump_mode=1` 启用 InferShape JSON 导出：

```
atc --model=model.onnx --dump_mode=1 --json=output.json
  │
  ├─ 解析模型为 ge::Graph
  ├─ GeGenerator::GenerateInfershapeGraph(graph)
  │    └─ 创建独立的 InferShapePass，通过 GEPass 执行
  ├─ DumpInfershapeJson(graph, json_path)
  │    ├─ 序列化为 protobuf ModelDef
  │    ├─ Pb2Json::Message2Json 转换为 JSON
  │    └─ 写入文件
  └─ 输出 JSON 包含所有算子推导后的 Shape 信息
```

## 9 关键数据结构

| 结构体 | 定义位置 | 说明 |
|--------|---------|------|
| `GeTensorDesc` | `graph/ge_tensor_desc.h` | 张量描述，同时携带 Origin 和 Storage 信息（Shape、Format、DataType、ShapeRange） |
| `GeShape` | `graph/ge_shape.h` | 纯维度数据结构，可承载 OriginShape 或 StorageShape |
| `StorageShape` | `exe_graph/runtime/storage_shape.h` | Origin + Storage 的复合描述体 |
| `InferShapeContext` | `exe_graph/runtime/infer_shape_context.h` | 算子 InferShape 函数的上下文参数 |
| `CtInferShapeContext` | `graph/ct_infer_shape_context.h` | 编译期扩展上下文，增加 InferenceContext 访问 |
| `InferSymbolShapeContext` | `exe_graph/runtime/infer_symbol_shape_context.h` | 符号化推导上下文 |
| `InferenceContext` | `graph/inference_context.h` | 编译期资源关联信息（handle shape、marks 等） |
| `ResourceContextMgr` | `graph/resource_context_mgr.h` | 资源上下文管理器，追踪资源依赖关系 |
| `ShapeInferenceRule` | `graph/utils/inference_rule.h` | 基于 JSON/编译二进制的 Shape 推导规则 |

## 10 关键文件索引

### API 层

| 文件 | 说明 |
|------|------|
| `api/acl/acl_op_executor/single_op/acl_op_executor.cpp` | aclopInferShape C 接口入口 |
| `api/acl/acl_op_executor/single_op/op.cpp` | aclopInferShapeImpl 核心实现 |
| `api/atc/omg.cc` | DumpInfershapeJson 实现 |
| `api/atc/main_impl.cc` | atc 命令行参数处理 |

### 编译器

| 文件 | 说明 |
|------|------|
| `compiler/graph/passes/shape_optimize/infershape_pass.h/.cc` | InferShapePass 核心实现 |
| `compiler/graph/passes/shape_optimize/infer_base_pass.h/.cc` | InferBasePass 基类 |
| `compiler/graph/passes/base_pass.h/.cc` | BaseNodePass + GEPass 遍历引擎 |
| `compiler/graph/preprocess/graph_prepare.h/.cc` | 图准备流水线，InferShapeForPreprocess 入口 |

### 运行时

| 文件 | 说明 |
|------|------|
| `runtime/v2/kernel/common_kernel_impl/infer_shape.h/.cc` | InferShape/FindInferShapeFunc 内核实现 |
| `runtime/v2/graph_builder/bg_infer_shape.h/.cc` | 执行图中 InferShape 节点构建 |
| `runtime/v2/lowering/pass/trust_out_tensor.cc` | TrustOutTensor 优化 Pass |
| `runtime/v2/lowering/pass/utils/pruner.cc` | 执行图剪枝 |

### 符号化推导

| 文件 | 说明 |
|------|------|
| `compiler/graph/optimize/symbolic/infer_symbolic_shape/symbolic_shape_inference.h/.cc` | 符号化推导主流程 |
| `compiler/graph/optimize/symbolic/infer_symbolic_shape/symbolic_info_post_processor.cc` | Merge Key 和 Guard 生成 |
| `compiler/graph/optimize/symbolic/infer_symbolic_shape/infer/*.cc` | 各算子符号化推导实现 |

### 元数据定义

| 文件 | 说明 |
|------|------|
| `inc/graph_metadef/exe_graph/runtime/infer_shape_context.h` | InferShapeContext 接口定义 |
| `inc/graph_metadef/exe_graph/runtime/storage_shape.h` | StorageShape 类型定义 |
| `inc/graph_metadef/exe_graph/runtime/infer_symbol_shape_context.h` | InferSymbolShapeContext 接口定义 |
| `inc/graph_metadef/graph/symbolizer/symbolic.h` | Expression/Symbol 符号表达式 |
