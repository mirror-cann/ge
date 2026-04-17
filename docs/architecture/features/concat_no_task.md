# Concat No Task 特性分析

## 1. 特性背景

### 1.1 问题场景

在深度学习模型中，`ConcatD`/`ConcatV2D` 算子用于将多个输入 Tensor 沿指定维度拼接为一个输出 Tensor。传统的 Concat 算子执行流程为：

```
输入A ──┐
输入B ──┼──► Concat Task ──► 输出(拼接结果)
输入C ──┘
```

其中 **Concat Task** 需要在昇腾 AI Core 上启动一个计算任务，通过 DataMove 指令将各输入数据搬运到输出地址的连续内存区域。

### 1.2 优化思路

当 Concat 算子的输入 Tensor 在内存中**天然连续排列**时，实际上不需要执行任何数据搬运操作——输出地址直接复用第一个输入的地址即可。Concat No Task 特性通过在编译期识别这种场景，将 Concat 算子标记为"虚拟算子"（Virtual Op），从而：

- **不生成硬件 Task**：跳过 AI Core 任务下发
- **不搬运内存**：输出直接复用输入内存地址
- **消除冗余计算**：避免无意义的数据搬移

## 2. 用户使用场景

### 2.1 典型场景：分布式训练中的 AllGather + Concat

在分布式训练中，多个卡通过 AllGather 收集各自的数据后，往往需要拼接为完整批次：

```
Card0: Data ──► AllGather ──┐
                             ├──► ConcatD ──► 完整Batch
Card1: Data ──► AllGather ──┘
```

AllGather 输出的数据在内存中已经按卡号顺序连续排列，Concat 只是逻辑上的拼接，物理上不需要搬运数据。

### 2.2 典型场景：分块计算后的结果合并

将大 Tensor 拆分到多个算子并行计算后合并结果：

```
Input ──► Split ──┬──► Relu ──┐
                  ├──► Relu ──┼──► ConcatD ──► Output
                  └──► Relu ──┘
```

当 Split 沿 batch 维度拆分、各分支计算不改变内存布局时，Concat 的输入天然连续。

### 2.3 适用条件

Concat No Task 优化需要同时满足以下严格条件：

| 条件类别 | 具体要求 |
|---------|---------|
| 算子类型 | 仅限 `ConcatD` 和 `ConcatV2D` |
| Shape 约束 | concat_dim 轴之前的所有维度值必须为 1 |
| 对齐约束 | concat_dim 轴原始尺寸必须是 align_shape 对应维度的整数倍（无 padding） |
| 输入约束 | 不能有 Scalar 输入；所有输入 Tensor 大小必须是 32 字节对齐 |
| 来源约束 | 不能有多个输入来源于同一个输出锚点 |
| 前驱节点 | 不能是 DATA、REFDATA、VARIABLE、CONSTANTOP、CONSTANT 等节点类型 |
| 前驱节点 | 不能包含子图（subgraph） |
| 前驱属性 | 不能有 continuous_input、continuous_output、reference 等属性 |
| 后继节点 | 后继节点不能已经是虚拟算子（已有 _no_task 属性） |
| 输出约束 | 输入不能同时是模型输出（NetOutput） |
| 内存类型 | 所有输入的内存类型必须一致 |
| LxFusion | 不能涉及 LxFusion（L1/L2/UB 地址、lxslice 算子） |
| Shape 模式 | 不支持 Unknown Shape（动态 Shape）场景 |
| 图模式 | 不支持 Single Op 场景和内存非连续分配场景 |

## 3. 对外接口

### 3.1 编译期属性标记

Concat No Task 通过以下 Graph 属性与系统其他模块交互：

| 属性名 | 类型 | 设置对象 | 说明 |
|-------|------|---------|------|
| `_no_task` | bool | Concat 算子 OpDesc | 标记该算子不生成硬件 Task |
| `_nopadding_continuous_input` | bool | Concat 算子 OpDesc | 标记输入为无 padding 的连续内存 |
| `_output_reuse_input` | bool | Concat 算子 OpDesc | 标记输出复用输入内存 |
| `_reuse_input_on_dim_index` | int64 | Concat 算子 OpDesc | 指定复用输入内存的维度索引（固定为 0） |
| `can_reused_for_concat_optimize` | bool | 前驱节点的输出 TensorDesc | 标记该输出已被 Concat 优化占用，不可再被其他 Concat 复用 |

### 3.2 Pass 注册

```
ConcatNotaskPass 注册为 O3 优化级别的 GraphPass：
REG_PASS_OPTION("ConcatNotaskPass").LEVELS(OoLevel::kO3);
```

在编译流程中，该 Pass 运行于 `OptimizeStage2` 的最后阶段，在子图合并完成后、内存冲突处理之前执行：

```
graph_manager.cc: OptimizeAfterMergeSubGraph()
  ├── ... (前期优化)
  ├── BufferPoolMemoryPass
  ├── ParallelGroupPass
  └── ConcatNotaskPass  ← 图稳定后执行
```

### 3.3 运行时行为

在运行时（RT1 和 RT2），标记了 `_no_task` 的算子会被特殊处理：

- **RT1 (Davinci)**：`TbeKernelHandle::NeedInit` 检测到 `_no_task` 属性后返回 false，跳过 Kernel 初始化
- **RT2 (V2)**：`IsVirtualOp` 函数检测 `_no_task` 属性，在 AICore Node Converter 中跳过 Task 生成

## 4. 具体实现

### 4.1 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                    编译期 (Compiler)                          │
│                                                             │
│  ┌──────────────────┐    ┌──────────────────────────────┐   │
│  │ ConcatNotaskPass │───►│ 属性标记                      │   │
│  │                  │    │  _no_task                    │   │
│  │ 校验链:           │    │  _nopadding_continuous_input │   │
│  │  ├─ InputCheck   │    │  _output_reuse_input         │   │
│  │  ├─ CheckConcatDim│   │  _reuse_input_on_dim_index   │   │
│  │  ├─ OutputCheck  │    └──────────────────────────────┘   │
│  │  └─ LxFusionCheck│                                       │
│  └──────────────────┘                                       │
└─────────────────────┬───────────────────────────────────────┘
                      │ 属性传递
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              内存分配 (Memory Assigner)                       │
│                                                             │
│  ┌─────────────────────┐    ┌──────────────────────────┐    │
│  │ BlockMemAssigner    │    │ GraphMemAssigner         │    │
│  │                     │    │                          │    │
│  │ 检测 NOPADDING_     │    │ 计算 continuous_type     │    │
│  │ CONTINUOUS_INPUT    │    │ kTypeInputNoPadding      │    │
│  │                     │    │                          │    │
│  │ no_assign_mem=true  │    │ 按 dim_index 计算        │    │
│  │ (不分配独立内存)     │    │ nopadding_size           │    │
│  └─────────────────────┘    └──────────────────────────┘    │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              Task 生成 (Task Generator)                       │
│                                                             │
│  检测到 _no_task 属性 → 跳过该节点的 Task 生成                │
│  MarkFirstAndLastOps 中跳过 notask 节点                      │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              运行时 (Runtime)                                 │
│                                                             │
│  RT1: TbeKernelHandle 跳过初始化                              │
│  RT2: AICoreNodeConverter 跳过转换                            │
│  输出地址直接复用第一个输入地址                                │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 ConcatNotaskPass 核心校验链

`ConcatNotaskPass::Run` 对图中每个 ConcatD/ConcatV2D 节点执行以下校验链，全部通过后才设置属性：

#### 4.2.1 场景排除

- **Single Op 场景**：单算子模式无需优化
- **内存非连续分配**：设置了 `ATTR_NAME_MEMORY_DISCONTIGUOUS_ALLOCATION` 的图跳过
- **Unknown Shape**：输入或输出包含动态 Shape 的算子跳过
- **Dynamic Shape 图**：所属图标记了动态 Shape 分区的跳过

#### 4.2.2 InputCheck（输入校验）

对每个输入锚点依次检查：

1. **IsScalarInput**：排除维度数为 0 的 Scalar 输入
2. **CheckTensorAlign**：多输入场景下，每个 Tensor 大小必须是 32 字节对齐
3. **HasSameSourceAnchor**：通过 `GetFirstOutAnchorNotInRefNode` 追溯 RefNode 链，确保没有多个输入追溯到同一个输出锚点
4. **IsPreNodeTypeValid**：通过 `GetFirstNotRefNode` 找到实际生产节点，排除 DATA/REFDATA/VARIABLE/CONSTANTOP/CONSTANT
5. **IsPreNodeWithSubgraph**：前驱节点不能包含子图实例
6. **IsPreOutAnchorCanReuseForConcatOptimize**：检查前驱输出 TensorDesc 的 `can_reused_for_concat_optimize` 属性，确保未被其他 Concat 占用
7. **IsPreOutAnchorValidMultiRef**：前驱输出如果同时连接到 NetOutput，则不能优化
8. **IsPreNodeAttrValid**：前驱节点不能有 continuous_input、continuous_output、reference、_no_task、_output_reuse_input、_nopadding_continuous_input 等属性，也不能有 atomic output
9. **IsSameInputMemType**：所有输入的内存类型必须一致（通过 `ATTR_NAME_OUTPUT_MEM_TYPE_LIST` 检查）

#### 4.2.3 CheckConcatDim（Concat 维度校验）

这是最核心的校验逻辑，确保 concat_dim 轴前面的维度全为 1：

```
原始格式 (如 NCHW) ──► 运行时格式 (如 NC1HWC0)
       │                        │
       │  GetTransferDims()     │
       │  (调用 FE 接口)         │
       ▼                        ▼
  src_to_dst_transfer_dims   dst_to_src_transfer_dims
  {0},{1,4},{2},{3}          {0},{1},{2},{3},{1}
```

- **GetAlignedShape**：调用 `transformer::TransferShapeUtils::GetAlignedShape` 获取对齐后的 shape
- **GetTransferDims**：调用 `transformer::TransferShapeUtils::TransferDims` 获取原始格式到运行时格式的轴映射关系
- **CheckRealConcatDim**：在运行时格式下找到实际的 concat_dim 轴，验证该轴之前的所有维度值都为 1
- **CheckConcatDimAlignment**：验证 concat_dim 轴原始尺寸是 align_shape 对应维度的整数倍（确保无 padding）

**CheckRealConcatDim 的关键逻辑**：

- 通过 `src_to_dst_transfer_dims[concat_dim][0]` 找到运行时格式中的 real_concat_dim
- 如果 real_concat_dim 是由合轴产生的（`dst_to_src_transfer_dims` 中对应多个源轴），需要额外验证：
  - real_concat_dim 轴之前的所有轴值都为 1
  - concat_dim 所在的合轴中、concat_dim 之前的值都为 1
- 如果 real_concat_dim 不是合轴产生的，只需验证之前的轴值都为 1

#### 4.2.4 OutputCheck（输出校验）

遍历 Concat 节点的所有后继节点：
- 如果后继是 Reshape 且有输出节点，则继续检查 Reshape 的输出节点
- 后继节点不能已有 `_no_task`、`_output_reuse_input`、`_nopadding_continuous_input` 属性

#### 4.2.5 LxFusionCheck（LxFusion 校验）

- **IsLxFusionMem**：检查输入/输出内存类型是否为 L1/L2/UB（这些是 LxFusion 使用的片上内存）
- **IsLxFusionOp**：检查算子名称是否包含 "lxslice"

### 4.3 属性设置

所有校验通过后，`SetAttrForConcatNotask` 执行以下操作：

```cpp
// 设置 Concat 算子自身属性
AttrUtils::SetBool(op_desc, ATTR_NAME_NOTASK, true);
AttrUtils::SetBool(op_desc, ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
AttrUtils::SetBool(op_desc, ATTR_NAME_OUTPUT_REUSE_INPUT, true);
AttrUtils::SetInt(op_desc, ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 0);

// 标记前驱节点的输出 TensorDesc 不可再被复用
for each input:
    AttrUtils::SetBool(output_tensor_desc, 
                       "can_reused_for_concat_optimize", false);
```

### 4.4 内存分配联动

#### 4.4.1 BlockMemAssigner

在 `AnalyzeSymbolMemReuseInfo` 中，当检测到节点具有 `ATTR_NAME_NOPADDING_CONTINUOUS_INPUT` 属性时：

```cpp
if (is_input_continuous) {
    symbol_mem_reuse_info_[symbol].no_assign_mem_ = true;
}
```

这意味着该符号（内存块）不会被独立分配内存，而是复用上游的内存地址。

`GetContinuousNodeLifeTimeBegin` 方法处理级联的 continuous input 场景：

```
a   b   c          (第一层)
|   |   |
d   e   f          (第二层)
|___|___|
    |
g   h   i          (第三层, h 是 continuous input)
|___|___|
    |
    j              (第四层, j 是 continuous input)
```

g 不能与 a/b/c 复用内存，因为 d/e/f 的内存会被 g 的地址替换（级联 continuous input）。因此 g 的生命期起点要追溯到 d/e/f 中的最早者。

#### 4.4.2 GraphMemAssigner

在 `GetContinuousType` 中识别 continuous 类型：

```
kTypeInputNoPadding = _nopadding_continuous_input && _output_reuse_input
kTypeOutputNoPadding = _nopadding_continuous_output && _output_reuse_input
```

在 `GetMemorySize` 中，对于 nopadding 类型：
- 通过 `_reuse_input_on_dim_index` 获取维度索引
- 计算 `nopadding_size`（实际数据大小）和 `tensor_size`（完整带 padding 大小）
- 内存分配器据此分配精确大小的内存块

#### 4.4.3 SetInputOutputOffsetPass

对于 no_task 的 Concat 节点，如果具有 `ATTR_NAME_CONTINUOUS_INPUT` 或满足 BufferFusion 条件，会调用 `SetOutputOffsetForConcat` 设置输出偏移，确保输出地址正确指向输入数据的起始位置。

### 4.5 Task 生成跳过

在 `TaskGenerator::MarkFirstAndLastOps` 中：

```cpp
bool attr_notask = false;
if (ge::AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, attr_notask) && attr_notask) {
    continue;  // 跳过 notask 节点，不纳入连续节点列表
}
```

这确保 no_task 节点不会被视为流中连续计算节点的一部分，不会影响首尾算子的标记。

### 4.6 运行时处理

#### 4.6.1 RT1 (Davinci Model)

在 `TbeKernelHandle::NeedInit` 中：

```cpp
bool attr_no_task = false;
const bool get_attr_no_task_flag = AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, attr_no_task);
if (get_attr_no_task_flag && attr_no_task) {
    GELOGI("Node[name:%s, type:%s] does not generate task, skip initialization.", ...);
    return false;  // 跳过 Kernel 初始化
}
```

#### 4.6.2 RT2 (V2 Engine)

在 `aicore_node_converter.cc` 的 `IsVirtualOp` 中：

```cpp
bool attr_no_task = false;
(void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, attr_no_task);
return attr_no_task;  // 返回 true 表示虚拟算子，跳过转换
```

## 5. 与其他优化的协同

### 5.1 与 Split No Task 的配合

Concat No Task 经常与 Split No Task 配合使用，形成"切分-计算-合并"的零拷贝流水线：

```
Input ──► Split(no_task) ──► 计算 ──► Concat(no_task) ──► Output
```

Split 沿 concat_dim 的反方向切分，输出地址直接指向输入地址的不同偏移；Concat 则将这些偏移地址视为连续内存，直接复用。

### 5.2 与内存复用的关系

Concat No Task 节点在内存复用检查中被特殊对待：

- `ReuseChecker` 中，具有 `_no_task` 属性的节点被视为 buffer_pool 类型
- `mem_reuse_strategy.cc` 中，nopadding continuous input 的节点参与特殊的内存复用策略

### 5.3 与地址刷新的关系

在 `MemLayoutConflictUtil` 中，Concat No Task 场景下需要考虑地址刷新：

```
data
 |
identity        (插入 identity 算子支持地址刷新)
 |
split(no_task, no_padding_continuous_output)
 /  \
op1  op2
```

当涉及用户输入且需要地址刷新时，会在 Data 和 Split 之间插入 Identity 算子。

## 6. 测试验证

单元测试文件位于 `tests/ge/ut/ge/graph/passes/concat_notask_pass_unittest.cc`，覆盖了以下场景：

| 测试用例 | 验证内容 |
|---------|---------|
| `allgather_connect_to_concat` | AllGather 输出连接到 Concat，验证属性设置正确 |
| `allgather_connect_to_concat_reshape` | AllGather → Reshape → Concat 链路 |
| 多组 RefData 测试 | 验证 RefNode 链追溯逻辑 |
| 不同 Format 测试 | ND、NCHW、NC1HWC0 等格式下的 dim 校验 |

## 7. 设计思考

### 7.1 为什么选择属性标记而非图改写

Concat No Task 采用属性标记而非删除节点的方式，原因是：

1. **保留调试信息**：Dump 功能需要保留算子的 OpDesc 和地址信息（见 `InitNoTaskAndDumpNeededNode`）
2. **保持图结构完整**：删除节点会破坏图的拓扑关系，影响其他 Pass 的执行
3. **支持动态场景**：属性标记可以在不同编译阶段灵活处理

### 7.2 为什么校验条件如此严格

Concat No Task 的校验条件多达十余项，这是因为：

1. **内存安全**：如果输入不连续却标记为 no_task，会导致读取到错误数据
2. **对齐约束**：昇腾硬件对内存对齐有严格要求，不满足 32B 对齐可能导致硬件异常
3. **级联影响**：一个 no_task 节点会影响下游的内存分配策略，错误标记会传播

### 7.3 为什么放在 Stage2 最后执行

注释明确说明"图稳定了再做 ConcatNotaskPass"：

- 在 Stage2 之前，图结构可能还在变化（子图合并、算子融合等）
- 前驱节点的属性可能在后续 Pass 中被修改
- 放在最后执行可以确保基于最终稳定的图结构做判断
