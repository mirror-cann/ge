# 常量折叠（Constant Folding）特性分析

## 1 特性概述

常量折叠是 GE 图编译器中一项核心的编译期优化。其核心思想是：在图编译阶段，识别出所有输入均为常量的算子节点，在主机侧提前完成计算，将计算结果写回为新的常量节点，从而消除运行时不必要的计算开销。

GE 的常量折叠不仅是简单的"常量表达式求值"，而是形成了一套包含维度计算、空张量替换、潜在常量标记、延迟生效等机制的完整优化体系，贯穿图预处理（Prepare）、优化阶段 1（OptimizeStage1）、优化阶段 2（OptimizeStage2）、自动融合前（BeforeAutofuse）等多个编译阶段。

### 1.1 解决的问题

在深度学习计算图中，大量节点的输入在编译期即可确定（如模型权重、超参数、Shape 相关操作），这些节点如果留到运行时在设备上执行，会带来两方面问题：

- **不必要的计算开销**：Shape、Rank、Size 等纯元信息操作完全可以在编译期完成
- **阻碍后续优化**：未折叠的常量节点阻挡了死代码消除、公共子表达式消除等优化的开展

### 1.2 功能范围

常量折叠特性包含以下能力：

| 能力 | 说明 |
|------|------|
| 标准常量折叠 | 将输入全为常量的算子替换为常量节点 |
| 维度计算折叠 | 对 Shape/Reshape/Transpose 等维度操作进行编译期求值 |
| 维度调整折叠 | 对 ExpandDims 等改变维度的算子进行原地优化并移除 |
| 空张量替换 | 将输出为空张量的算子替换为空常量节点 |
| 潜在常量标记 | 标记当前部分输入为非常量、但未来可能变为全常量的节点 |
| 潜在常量生效 | 在图遍历结束后，统一将已标记的潜在常量节点执行折叠 |

---

## 2 用户使用场景

### 2.1 离线编译场景（atc）

用户使用 atc 工具将 ONNX/PB 模型编译为 OM 文件时，常量折叠默认在 O1 及以上优化等级启用。用户可通过配置参数控制开关：

```
atc --model=model.onnx --output=model --framework=5 \
    --ge.oo.level=O1 --ge.oo.constantFolding=true
```

典型收益场景：模型中包含大量用于动态 Shape 推断的辅助计算（如 Shape→Gather→Concat→Reshape 链路），这些在静态 Shape 场景下完全可以预计算，常量折叠能将其全部消除。

### 2.2 在线编译场景（aclgrphBuildModel）

使用 ACL Graph Builder API 构建模型时，通过 `aclgrphBuildInitialize` 或 `aclgrphBuildModel` 的配置参数控制：

```cpp
// 全局级别配置
std::map<ge::AscendString, ge::AscendString> global_options = {
    {ge::ir_option::OO_LEVEL, "O1"},
    {ge::ir_option::OO_CONSTANT_FOLDING, "true"}
};
aclgrphBuildInitialize(global_options);

// 图级别配置（优先级更高）
std::map<ge::AscendString, ge::AscendString> build_options = {
    {ge::ir_option::OO_CONSTANT_FOLDING, "true"}
};
aclgrphBuildModel(graph, build_options, modelBufferData);
```

### 2.3 调试场景

当用户怀疑常量折叠导致结果异常时，可以关闭该优化进行对比验证：

```cpp
{ge::ir_option::OO_CONSTANT_FOLDING, "false"}
```

关闭后，Size、Shape、ShapeN、Rank 等算子不会被折叠删除，运行时仍将在设备上执行。`ge_deleted_op.cc` 中的 `GeDeletedOp` 机制会针对这些算子给出清晰的错误提示，帮助用户定位问题。

### 2.4 用户指定跳过折叠

框架侧（如 TensorFlow 的 `_grappler_do_not_remove` 属性）或用户可通过设置节点属性 `_do_not_constant_folding` 来阻止特定节点被常量折叠。这为需要保留特定节点的场景提供了精细控制手段。

---

## 3 对外接口

### 3.1 配置参数

| 参数键 | 参数值 | 配置入口 | 说明 |
|--------|--------|----------|------|
| `ge.oo.constantFolding` | `"true"` / `"false"` | aclgrphBuildInitialize, aclgrphBuildModel, atc | 控制常量折叠优化开关 |
| `ge.oo.level` | `"O1"` / `"O2"` / `"O3"` | 同上 | 优化等级，O1 及以上默认启用常量折叠 |

配置参数定义位于 `inc/graph_metadef/external/ge_common/ge_api_types.h`，常量名 `OO_CONSTANT_FOLDING`，实际配置键为 `"ge.oo.constantFolding"`。

选项注册位于 `base/common/option_register.cc`：

```
REG_OPTION(OO_CONSTANT_FOLDING)
    .LEVELS(OoLevel::kO1)
    .DEFAULT_VALUES({{OoLevel::kO1, "true"}, {OoLevel::kO3, "true"}})
    .CHECKER(OoInfoUtils::IsSwitchOptValueValid)
    .VISIBILITY({OoEntryPoint::kSession, OoEntryPoint::kIrBuild, OoEntryPoint::kAtc})
    .SHOW_NAME(OoEntryPoint::kAtc, "oo_constant_folding", OoCategory::kModelTuning)
```

选项在 O1 和 O3 优化等级下默认开启，支持 Session、IR Build 和 ATC 三个入口配置。

### 3.2 节点属性接口

| 属性名 | 作用 | 设置方 |
|--------|------|--------|
| `ATTR_NO_NEED_CONSTANT_FOLDING` | 标记节点无需常量折叠 | GE 内部 Pass |
| `ATTR_NAME_DO_NOT_CONSTANT_FOLDING` | 标记用户指定节点不做常量折叠 | 框架 Parser / 用户 |
| `ATTR_NAME_POTENTIAL_CONST` | 标记节点为潜在常量 | GE 常量折叠 Pass |
| `ATTR_NAME_POTENTIAL_WEIGHT` | 存储潜在常量的权重值 | GE 常量折叠 Pass |
| `ATTR_NAME_POTENTIAL_WEIGHT_INDICES` | 存储潜在常量的输出索引 | GE 常量折叠 Pass |
| `_is_from_constant_folding` | 标记常量节点由常量折叠产生 | GE 常量折叠 Pass |
| `ATTR_NAME_IS_INSERTED_BY_GE` | 标记节点由 GE 内部插入 | GE 内部 Pass |

属性定义位于 `inc/graph_metadef/graph/debug/ge_attr_define.h`。

### 3.3 工具类接口

`GraphOptimizeUtility::ConstantFolding`（位于 `compiler/graph/manager/util/graph_optimize_utility.cc`）提供了单节点级别的常量折叠接口，供其他模块（如权重压缩判断 `WeightCompressJudge`）按需调用。该接口依次执行 ConstantFoldingPass → DimensionComputePass → ReplaceWithEmptyConstPass。

---

## 4 具体实现

### 4.1 整体架构

常量折叠采用 Pass 链式执行模式，通过 GE 的 Pass 管理框架驱动。核心实现集中在 `compiler/graph/passes/standard_optimize/constant_folding/` 目录下，由 7 个 Pass 和配套的基础设施组成：

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
                         │  折叠基础操作：            │
                         │  创建Const节点、删除原节点  │
                         │  重连数据边、保留控制边     │
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

### 4.2 Pass 继承体系

常量折叠的 Pass 体系采用三层继承结构，逐层增加能力：

**BaseNodePass** → 定义了节点级 Pass 的基本框架，包括节点遍历、重 Pass（RePass）、节点删除跟踪等机制。

**FoldingPass** → 继承 BaseNodePass，实现了核心的"折叠"动作（`FoldingPass::Folding`），包括：
- 收集被折叠节点的下游锚点连接关系（`GetIndexAndPeerInDataAnchors`）
- 处理 Switch/RefSwitch 类型的输入节点（断开数据边、插入 Identity 节点保持控制依赖）
- 创建新的 Const 节点替换原节点输出（`AddConstNodeToGraph`）
- 将新 Const 节点连接到原节点的所有下游消费者
- 隔离并删除原节点及其变为孤立的输入常量节点
- 传递流标签（Stream Label）属性以保持流规划的正确性

**PotentialFoldingPass** → 继承 FoldingPass，引入"潜在常量"概念和计算调度机制，由 `PotentialFoldingPass::Run` 统一编排。

**ConstantFoldingPass / DimensionComputePass / ReplaceWithEmptyConstPass** → 继承 PotentialFoldingPass，各自实现不同的计算策略和判断逻辑。

**DimensionAdjustPass** → 直接继承 BaseNodePass，处理维度调整类算子（如 ExpandDims），在主机侧完成计算后直接隔离删除，不走完整的常量折叠流程。

**PotentialConstTakenEffectPass** → 直接继承 FoldingPass，负责在图遍历结束后统一处理所有已标记的潜在常量节点。

### 4.3 核心 Pass 详解

#### 4.3.1 ConstantFoldingPass — 标准常量折叠

文件：`compiler/graph/passes/standard_optimize/constant_folding/constant_folding_pass.h/cc`

这是常量折叠的主入口 Pass，处理所有输入均为常量的节点。其执行流程如下：

1. **前置检查**
   - 检查用户是否设置了 `_do_not_constant_folding` 属性，若是则跳过
   - 检查节点是否标注了 `ATTR_NO_NEED_CONSTANT_FOLDING`，若是则跳过
   - 检查节点是否为潜在空常量（所有输出 Shape 为空），若是则交给 ReplaceWithEmptyConstPass 处理

2. **输入验证**
   - 确认节点的所有输入均为常量节点（通过 `OpDescUtils::GetConstInputNodeAndAnchor`）
   - 若存在非常量输入但该节点被标记为"潜在常量"，则置 `need_fold_ = false` 并返回 NOT_CHANGED（本轮不折叠，但保留标记）
   - 从常量输入节点提取权重值（通过 `OpDescUtils::GetWeightsFromNodes`）

3. **内存优先策略检查**
   - 当配置了 MemoryPriority 策略时，若输入常量节点的 Shape 较大（>8）且被多个下游共享，则跳过折叠。因为折叠会复制一份常量数据，在内存受限场景下得不偿失

4. **计算执行（两级回退策略）**
   - **第一级：AICPU 算子内核**（`ComputeWithHostCpuKernel`）
     - 尝试通过 `aicpu_ascend_kernel` 引擎获取算子的 Host CPU 实现
     - 通过 `OpKernelRegistry` 创建算子实例，由 `HostCpuEngine` 执行
     - 这一级支持最广泛的算子类型，运行时加载 `libconstant_folding_ops.so`
   - **第二级：GE 内置内核**（`ComputeWithBuiltInKernel`）
     - 若 AICPU 不支持该算子，回退到 GE 内置的 Host Kernel
     - 通过 `KernelFactory` 按算子类型查找注册的 Kernel（`folding_pass::GetKernelByType`）
     - GE 内置 Kernel 覆盖了约 40 种常见算子

5. **折叠替换**
   - 计算成功后，由 `FoldingPass::Folding` 完成图结构变换
   - 新建的 Const 节点会被标记 `_is_from_constant_folding=true`，用于后续流程识别

6. **性能统计**
   - 分别记录 AICPU 内核和 GE 内置内核的折叠耗时和调用次数
   - 在 `GraphManager::OptimizeStage1_2` 中汇总输出性能追踪日志

Pass 注册宏：
```
REG_PASS_OPTION("ConstantFoldingPass").SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```
该 Pass 受 `ge.oo.constantFolding` 开关控制。

#### 4.3.2 DimensionComputePass — 维度计算折叠

文件：`compiler/graph/passes/standard_optimize/constant_folding/dimension_compute_pass.h/cc`

专门处理维度相关的计算操作（如 Shape、Reshape、Transpose 等）。与 ConstantFoldingPass 的区别在于：

- **仅使用 GE 内置 Kernel**：通过 `folding_pass::GetKernelByType` 获取 Kernel 并计算，不尝试 AICPU 内核
- **支持标记但不折叠模式**：可通过构造参数 `need_fold` 控制是否只做计算标记（在预处理阶段以 `need_fold=false` 模式运行，只标记潜在常量不实际折叠）
- 支持与 PotentialFoldingPass 的潜在常量机制配合

在预处理阶段（`GraphPrepare::ComputeConstantShape`），DimensionComputePass 以 `need_fold=false` 模式运行，目的是先利用维度计算确定 Shape 信息，为后续的 InferShape 提供更准确的输入。

Pass 注册宏：
```
REG_PASS_OPTION("DimensionComputePass").LEVELS(OoLevel::kO1).SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```

#### 4.3.3 DimensionAdjustPass — 维度调整折叠

文件：`compiler/graph/passes/standard_optimize/constant_folding/dimension_adjust_pass.h/cc`

处理 ExpandDims 等通过常量参数调整维度的算子。工作流程：

1. 获取节点原始类型，查找对应的 GE 内置 Kernel
2. 检查是否为未知 Shape，未知 Shape 的节点跳过
3. 调用 Kernel 的 `Compute(node)` 接口进行维度调整计算
4. 删除无用的常量轴参数输入节点
5. 通过 `IsolateAndDeleteNode` 将节点隔离删除，并按 `{0}` 的 IO 映射重连数据边（即只保留数据输入直连数据输出）

DimensionAdjustPass 不走完整的常量折叠替换流程（不创建 Const 节点），而是直接完成维度推断后将节点简化为直连。

Pass 注册宏：
```
REG_PASS_OPTION("DimensionAdjustPass").LEVELS(OoLevel::kO1).SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```

#### 4.3.4 ReplaceWithEmptyConstPass — 空张量替换

文件：`compiler/graph/passes/standard_optimize/constant_folding/replace_with_empty_const_pass.h/cc`

识别输出为空张量（Shape 中包含维度 0，如 `[0, 3, 224, 224]`）的算子，将其替换为空常量节点。

排除规则（不会被替换的节点类型）：
- Const/ConstantOp/FileConstant 等常量类节点
- Data 类节点
- NetOutput 节点
- 控制流算子（Switch、Merge、Enter、NextIteration、Exit、LoopCond 等）
- 资源算子（Stack、StackPop、StackPush）
- Hcom 集合通信算子
- 无输出描述的节点
- GE 内部插入的节点（`ATTR_NAME_IS_INSERTED_BY_GE` 标记）

此 Pass 也支持 `need_fold` 参数控制是否只做标记。

Pass 注册宏：
```
REG_PASS_OPTION("ReplaceWithEmptyConstPass").LEVELS(OoLevel::kO1).SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```

#### 4.3.5 PotentialConstTakenEffectPass — 潜在常量生效

文件：`compiler/graph/passes/standard_optimize/constant_folding/potential_const_taken_effect_pass.h/cc`

这是一个特殊 Pass，其 `Run` 方法为空操作（直接返回 SUCCESS），实际工作在 `OnFinishGraph` 回调中完成。

设计意图：在图遍历过程中，某些节点的部分输入在当前轮次还不是常量（例如来自 Shape 推断的中间结果），DimensionComputePass 等会将其标记为"潜在常量"并缓存权重。当图遍历完成后（所有 Pass 已执行完毕），潜在常量的输入可能已在前序轮次被折叠为常量。此时 PotentialConstTakenEffectPass 在 `OnFinishGraph` 中统一扫描所有标记为潜在常量的节点，执行延迟折叠。

处理流程：
1. 遍历图中所有节点，找到带有 `ATTR_NAME_POTENTIAL_CONST` 标记的节点
2. 从属性中读取缓存的潜在权重（`ATTR_NAME_POTENTIAL_WEIGHT`）
3. 若权重存在，调用 `FoldingPass::Folding` 执行折叠
4. 若权重缺失，清除潜在常量相关属性并记录警告
5. 收集所有需要重 Pass 的节点，传递给下一轮 Pass 执行

Pass 注册宏：
```
REG_PASS_OPTION("PotentialConstTakenEffectPass").LEVELS(OoLevel::kO1).SWITCH_OPT(ge::OO_CONSTANT_FOLDING);
```

### 4.4 潜在常量机制

潜在常量（Potential Const）是 GE 常量折叠的核心创新之一，用于解决"当前输入不全为常量但后续可能变为常量"的场景。

相关属性和工具类：

| 组件 | 位置 | 说明 |
|------|------|------|
| `ConstantUtils` | `inc/graph_metadef/graph/utils/constant_utils.h` | 潜在常量标记/查询/清除工具类 |
| `ATTR_NAME_POTENTIAL_CONST` | `inc/graph_metadef/graph/debug/ge_attr_define.h` | 标记节点为潜在常量 |
| `ATTR_NAME_POTENTIAL_WEIGHT` | 同上 | 缓存潜在常量的权重值 |
| `ATTR_NAME_POTENTIAL_WEIGHT_INDICES` | 同上 | 标记哪些输出索引为潜在常量 |
| `_source_pass_of_potential_const` | `potential_folding_pass.cc` | 记录来源 Pass 名，防止跨 Pass 误操作 |

机制流程：

```
ConstantFoldingPass / DimensionComputePass
         │
         │ (节点部分输入为非常量)
         ▼
  ComputePotentialWeight()
    → 计算成功，得到输出权重
    → need_fold = false (本轮不折叠)
         │
         ▼
  UpdatePotentialConstMark()
    → MarkPotentialConstWithPassName()
      → ConstantUtils::MarkPotentialConst()
      → 设置 _source_pass_of_potential_const
         │
         ▼
  (多轮 InferShape + ConstantFolding 后，
   部分输入被其他 Pass 折叠为常量)
         │
         ▼
  PotentialConstTakenEffectPass::OnFinishGraph()
    → 读取 ATTR_NAME_POTENTIAL_WEIGHT
    → FoldingPass::Folding() 执行折叠
```

`PotentialFoldingPass` 中的 `IsCurPassSameWithSource` 方法确保只有最初标记潜在常量的 Pass 才能更新或清除该标记，避免不同 Pass 之间的干扰。

### 4.5 计算引擎

常量折叠的计算引擎分为两层：

#### 4.5.1 GE 内置 Host Kernel

位于 `compiler/host_kernels/`，通过 `KernelFactory` 注册和创建。`Kernel` 基类定义了三种 Compute 接口：

- `Compute(OpDescPtr, inputs, outputs)` — 基于输入张量计算输出张量，用于 ConstantFoldingPass 和 DimensionComputePass
- `Compute(NodePtr, outputs)` — 基于节点信息计算输出，部分 Kernel 使用
- `Compute(NodePtr)` — 仅修改节点属性，用于 DimensionAdjustPass

已注册的 Host Kernel 按类别分布如下：

| 类别 | 目录 | 算子列表 |
|------|------|----------|
| 数组操作 | `host_kernels/array_ops/` | Reshape, Squeeze, SqueezeV3, Unsqueeze, UnsqueezeV3, ExpandDims, Rank, Shape, ShapeN, Size, Identity, Empty, BroadcastArgs, BroadcastGradientArgs, GatherShapes |
| 逐元计算 | `host_kernels/elewise_calculation_ops/` | Add, Sub, Mul, FloorDiv, FloorMod, Maximum, Greater, Rsqrt, Cast |
| 选择操作 | `host_kernels/selection_ops/` | Slice, SliceD, StridedSlice, GatherV2, Range |
| 变换操作 | `host_kernels/transformation_ops/` | Transpose, Permute, Transdata, FlattenV2 |
| 拼接拆分 | `host_kernels/split_combination_ops/` | ConcatV2, ConcatOffset, Pack, Unpack |
| 填充操作 | `host_kernels/pad_ops/` | Fill |
| 规约操作 | `host_kernels/reduce_ops/` | ReduceProd |
| 数据流 | `host_kernels/data_flow_ops/` | DynamicStitch |
| 自定义 | `host_kernels/custom_ops/` | ReFormat, SsdPriorBox |

每个 Kernel 通过 `REGISTER_COMPUTE_NODE_KERNEL` 宏注册到 `KernelFactory`，运行时按算子类型查找。

#### 4.5.2 AICPU Host CPU 引擎

作为 GE 内置 Kernel 的补充，通过 AICPU 引擎执行算子。位于 `runtime/v2/engine/aicpu/` 和 `runtime/v1/hybrid/node_executor/host_cpu/`。

加载路径为 `libconstant_folding_ops.so`，由 OPP（Operator Package）提供，包含更广泛的算子实现。`compiler/engines/cpu_engine/cpu_engine/constant_folding_stub/constant_folding_ops_stub.cpp` 是编译期的桩库（无实际实现），运行时由真正的 OPP 库替换。

`HostCpuEngine` 负责：
- 加载 `libconstant_folding_ops.so` 动态库
- 通过 `OpKernelRegistry` 创建算子实例
- 调用 `RunHostCpuKernel` 执行计算

### 4.6 编译流程集成

常量折叠在 GE 图编译的多个阶段被调用，形成多轮迭代优化：

```
GraphPrepare::ComputeConstantShape (预处理阶段)
  │
  ├── ReplaceWithEmptyConstPass (need_fold=false, 仅标记)
  ├── DimensionComputePass (need_fold=false, 仅标记)
  ├── ConstantClipPass
  ├── ConstantFoldingPass
  └── InferValueRangePass

GraphManager::OptimizeStage1_2 (优化阶段1)
  │
  ├── ReplaceWithEmptyConstPass
  ├── DimensionComputePass
  ├── ConstantClipPass
  ├── ConstantFoldingPass
  └── DimensionAdjustPass

GraphManager::OptimizeStage2 (优化阶段2, 合并子图后)
  │
  ├── ConstantFoldingPass
  ├── ReshapeRemovePass
  ├── CondRemovePass
  ├── AssignRemovePass
  └── DimensionAdjustPass

GraphOptimizerBeforeAutofuse (自动融合前)
  │
  └── ConstantFoldingPass
```

多阶段调用的设计考虑：
- **预处理阶段**以标记模式运行 DimensionComputePass，先完成维度推断再执行标准折叠
- **优化阶段1**是常量折叠的主战场，包含所有 Pass 的完整执行
- **优化阶段2**对合并子图后的图再做一次常量折叠，消除子图合并引入的新常量计算
- **自动融合前**额外执行一次常量折叠，确保融合 Pass 面对的是最优化的图结构

### 4.7 与其他优化的协作

常量折叠与多个其他优化 Pass 存在协作关系：

- **ConstantClipPass**：在常量折叠前处理权重裁剪，插入的 Min/Max 节点会被后续的 ConstantFoldingPass 再次折叠
- **DeadCodeElimination**：常量折叠产生的孤立常量节点会被死代码消除清除
- **CommonSubexpressionElimination**：共享同一常量输入的场景下，常量折叠后公共子表达式消除可以进一步去重
- **WeightCompressJudge**：权重压缩判断在执行前调用 `GraphOptimizeUtility::ConstantFolding` 先对权重相关节点做常量折叠
- **DeleteNoConstFoldingFusionPass**：FE 量化 Pass，在融合阶段删除 `AscendWeightQuant` 算子上的 `ATTR_NO_NEED_CONSTANT_FOLDING` 属性，使其可以被常量折叠处理
- **DecomposeLargeConstPass**：大常量拆分 Pass 会继承 `_is_from_constant_folding` 属性到拆分后的新常量节点
- **AscIrLowerer**：在 IR Lowering 阶段，对来自常量折叠的控制边做特殊处理（允许移除以获得更多融合机会）
- **CtrlEdgeTransferPass**：常量折叠后形成的控制边关系需要由控制边转移 Pass 清理
