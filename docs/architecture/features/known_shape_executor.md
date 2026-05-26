# GE 静态执行器（Known Shape Executor）特性分析

## 1. 特性背景

静态执行器是 GE 运行时中负责加载和执行静态shape图或者子图的核心组件。它以 `DavinciModel` 类为载体，提供两种使用模式：

1. **独立静态模型模式**：整个模型编译为单个 OM 文件，通过 `ModelManager` 直接加载执行

2. **V2 Runtime Kernel 集成模式**：在新一代 V2 运行时中，静态子图以 Kernel 的形式嵌入执行图，通过 Kernel 注册机制管理生命周期（创建、参数刷新、执行、工作区更新），实现与动态子图的协同调度

## 2. 用户使用场景

### 2.1 场景一：离线模型推理（ACL 模式）

用户通过 ATC 工具将模型转换为 OM 文件后，在推理侧通过 ACL API 加载并执行：

```
模型文件(.om) → aclmdlLoadFromFile → aclmdlExecute → 输出结果
```

底层执行路径：`ModelManager::LoadModelOffline` → `DavinciModel::Init` → `DavinciModel::NnExecute`

### 2.2 场景二：通过 GeSession 接口执行

用户通过 GE V2 API 提供的 `GeSession` 类加载和执行计算图，这是 GE 运行时的高层编程接口．

### 2.3 场景三：V2 Runtime Kernel 集成

在新一代 V2 运行时中，静态子图以 Kernel 的形式集成到执行图中，通过注册的 Kernel 函数完成生命周期管理：

- `DavinciModelCreate`：创建并初始化 DavinciModel
- `DavinciModelUpdateArgs`：刷新输入输出地址
- `DavinciModelExecute`：触发设备侧执行
- `DavinciModelUpdateWorkspaces`：更新工作内存基址
- `DavinciModelGetRunAddress`：获取运行时内存地址（用于 Kernel 间地址依赖）

## 3. 对外接口

### 3.1 DavinciModel 核心接口

`DavinciModel` 是静态执行器的核心类，封装了模型加载、内存管理、任务分发和执行的全流程：

**加载阶段**：
- `Init(ModelParam, outer_fm_mem)`：模型初始化入口，完成内存分配、资源创建、任务下沉（Task Sink）
- `SetKnownNode(bool)`：标记是否为混合执行模式下的已知形状子图节点

**执行阶段**：
- `NnExecute(stream, async_mode, input_tensor, output_tensor)`：模型执行入口
- `UpdateKnownNodeArgs(inputs, outputs)`：为已知形状子图刷新输入输出地址（混合执行模式专用）
- `CopyModelData`：拷贝输入数据到设备侧
- `CopyOutputData`：拷贝输出数据回用户缓冲区

**资源管理**：
- `GetRtModelHandle()`：获取底层 RT 模型句柄
- `GetLogicalMemAllocation()`：获取逻辑内存分配表
- `UpdateHbmFmMemBases`：更新 HBM 特征图内存基址

### 3.2 V2 Kernel 注册接口

```
REGISTER_KERNEL(DavinciModelCreate)       // 创建模型
REGISTER_KERNEL(DavinciModelCreateV2)     // V2 版本创建
REGISTER_KERNEL(DavinciModelUpdateArgs)   // 更新参数
REGISTER_KERNEL(DavinciModelExecute)      // 执行模型
REGISTER_KERNEL(DavinciModelUpdateWorkspaces) // 更新工作区
REGISTER_KERNEL(DavinciModelGetRunAddress)    // 获取运行时地址
```

## 4. 架构设计

### 4.1 类层次结构

```
Executor (base/common/model/executor.h)
  ├── ModelExecutor (runtime/v1/graph/execute/model_executor.h)
  │     └── 委托给 ModelManager -> DavinciModel
  │
DavinciModel (runtime/v1/graph/load/model_manager/davinci_model.h)
  ├── 独立模型: known_node_ = false
  └── 已知形状子图: known_node_ = true
        └── V2 runtime kernel

```

## 5. 核心实现

### 5.1 模型加载流程（DavinciModel::Init）

模型加载是静态执行器最复杂的阶段，涉及内存分配、资源创建、任务分发等多个子步骤。

```
DavinciModel::Init()
│
├── InitRuntimeParams()
│     从 GeModel 中提取运行时参数：内存布局、任务定义、流配置等
│
├── InitWeightMem()
│     加载权重数据到设备侧 HBM
│
├── InitFixedFeatureMap()
│     设置固定（不可刷新）特征图内存
│     这部分内存在模型生命周期内地址不变
│
├── InitFeatureMapAndP2PMem()
│     设置可刷新特征图内存
│     支持分段内存（sub memory）管理
│
├── PreProcessFileConstants()
│     处理外部权重文件（FileConstant 节点）
│     支持权重合并（Combined Weights）优化
│
├── InitIoNodes()
│     初始化 Data 和 NetOutput 节点
│     配置零拷贝（Zero-Copy）内存映射
│
├── InitRuntimeResource()
│     创建 RT 模型句柄（rtModel_t）
│     创建执行流（aclrtStream）、事件（Event）、标签（Label）
│
├── TransAllVarData()
│     将变量（Variable）数据传输到设备侧
│
├── InitNodes()
│     初始化所有计算节点
│     加载 TBE Kernel 句柄、注册算子实现空间
│
└── DoTaskSink()          ★ 关键步骤：任务下沉到设备
      │
      ├── BindModelStream()
      │     将逻辑流绑定到物理 RT Stream
      │
      ├── InitTaskInfo()
      │     从 ModelTaskDef 创建 TaskInfo 对象
      │     初始化 ModelArgsManager（参数管理器）
      │
      ├── LoadWithQueue()
      │     如果配置了队列调度，设置队列执行路径
      │
      ├── DistributeTask()
      │     通过 rtPersistentTaskLaunch 将任务分发到设备侧
      │     所有 Kernel 的 Launch 参数在此阶段预置到设备
      │
      ├── UpdateStaticModelArgsByFm()
      │     用特征图地址初始化参数刷新表
      │
      └── aclmdlRIBuildEnd()
            标记 RT 模型构建完成
```

**Task Sink 的设计意义**：将编译期生成的所有任务（Task）预先分发到设备侧，执行时只需触发 `rtModelExecute`，无需每次执行都进行 Kernel Launch。这是静态执行器高性能的核心保障——消除了 Host 侧的 Kernel 启动开销。

### 5.2 模型执行流程（DavinciModel::NnExecute）

```
DavinciModel::NnExecute(stream, async_mode, input_tensor, output_tensor)
│
├── InitModelStream(stream)
│     设置执行流
│
├── CopyModelData(input_tensor, output_tensor)
│     │
│     ├── UpdateAllNodeArgs()
│     │     更新所有 Kernel 的 Launch 参数
│     │     包括输入输出地址、形状信息等
│     │
│     └── CopyInputForNoZeroCopy()
│           对于非零拷贝的输入，执行 H2D 数据拷贝
│
├── rtModelExecute(rt_model_handle_, rt_model_stream_, 0U)  ★ 设备侧执行
│     或 rtModelExecuteSync()（MDC 场景带超时控制）
│
├── rtStreamSynchronizeWithTimeout()
│     等待执行完成（内建流场景）
│
├── CopyOutputData(output_tensor)
│     将输出数据拷贝回用户缓冲区
│     零拷贝模式下跳过此步骤
│
└── UpdateOutputTensorShape()
      更新输出张量形状（动态 Shape 场景）
```

### 5.3 地址刷新机制（核心创新）

在混合执行模式下，已知形状子图的输入输出地址在每次迭代时都可能变化。静态执行器通过**逻辑内存分配表 + 活跃地址映射**的机制实现高效的地址刷新。

#### 5.3.1 数据结构

`DavinciModel` 维护以下关键数据结构：

- `logical_mem_allocations_`：逻辑内存分配表，记录每个逻辑内存区域的类型（INPUT/OUTPUT/FEATURE_MAP）、大小、命中次数等元信息
- `allocation_ids_to_active_base_addr_`：活跃地址映射表，将 allocation_id 映射到当前执行的实际设备地址
- `refreshable_input_index_and_allocation_ids_`：可刷新输入索引到 allocation_id 的映射
- `refreshable_output_index_and_allocation_ids_`：可刷新输出索引到 allocation_id 的映射
- `refreshable_fm_index_and_allocation_ids_`：可刷新特征图索引到 allocation_id 的映射

#### 5.3.2 刷新流程

```
DavinciModel::UpdateKnownNodeArgs(inputs, outputs)
│
├── ConstructActiveMemBaseAddrsForKnownNode(ret_up, inputs, outputs)
│     │
│     ├── 更新 FM 地址
│     │     遍历 refreshable_fm_index_and_allocation_ids_
│     │     将 runtime_param_.fm_memory_infos 中的地址写入活跃地址表
│     │
│     ├── 更新输入地址
│     │     遍历 refreshable_input_index_and_allocation_ids_
│     │     将用户传入的 inputs[i] 设备地址写入活跃地址表
│     │     首次执行时包含非冻结输入，后续使用 zero_copy_no_frozen
│     │
│     └── 更新输出地址
│           遍历 refreshable_output_index_and_allocation_ids_
│           将用户传入的 outputs[i] 设备地址写入活跃地址表
│
└── args_manager_.UpdateForExecute(ret_up, rt_model_stream_)
      将更新后的活跃地址表拷贝到设备侧
      通过 UpdateModelParam Kernel 实现高效刷新
      ret_up 决定刷新策略（全量刷新 vs 增量刷新）
```

**设计精妙之处**：

1. **增量刷新策略**：`ret_up` 变量记录需要刷新的最大策略级别，`args_manager_` 根据此值决定只拷贝发生变化的地址，最小化 H2D 带宽消耗
2. **冻结输入优化**：对于地址不变的输入（Frozen Inputs），在首次执行后从刷新列表中排除，避免不必要的地址更新
3. **零拷贝支持**：用户提供的 I/O 缓冲区直接映射到设备 Kernel 参数，无需中间拷贝

### 5.4 known_node_ 标志的影响

`known_node_` 是区分独立静态模型和混合执行模式下已知形状子图的关键标志。设置此标志后（通过 `SetKnownNode(true)`），`DavinciModel` 的行为发生以下变化：

| 行为 | known_node_ = false | known_node_ = true |
|------|---------------------|-------------------|
| Session ID 获取 | 使用 `runtime_param_.graph_id` | 使用 `runtime_param_.root_graph_id` |
| 地址刷新 | 使用 `UpdateAllNodeArgs` | 使用 `UpdateKnownNodeArgs` |
| 特征图基址 | 固定不可刷新 | 可刷新（`feature_base_refreshable_ = true`） |
| 错误追踪清理 | 执行 | 跳过 |
| 变量初始化 | 标准路径 | 特殊路径 |
| 内存分段 | 可能合并为单段 | 保持分段结构 |

### 5.5 ModelArgsManager 参数管理

`ModelArgsManager` 是静态执行器中管理 Kernel 启动参数的核心组件，负责：

1. **初始化阶段**：从 `ModelTaskDef` 中解析所有任务的参数布局，建立逻辑地址到设备参数的映射
2. **执行阶段**：根据活跃地址表更新设备侧参数，通过 `UpdateModelParam` Kernel 实现高效刷新
3. **策略管理**：维护 `id_to_policy` 映射，支持全量刷新（all-one-time）和增量刷新两种策略

### 5.6 内存管理策略

静态执行器采用分层内存管理策略：

```
设备侧内存布局
│
├── 权重内存（weights_mem_base_）
│     模型权重数据，加载后地址固定
│
├── 固定特征图内存（fixed_mem_base_）
│     不可刷新的特征图内存
│     在模型生命周期内地址不变
│
├── 可刷新特征图内存（mem_base_）
│     支持运行时地址刷新
│     分段场景下为首个 refreshable FM 段的地址
│
├── 零拷贝 I/O 内存
│     用户提供的输入输出缓冲区
│     地址通过 args_manager_ 刷新
│
└── 变量内存（var_mem_base_）
      模型变量（如 BatchNorm 的 running mean/var）
```

## 6. 编译器侧支持

### 6.1 动静 Shape 图划分

`DynamicShapePartitioner` 负责将计算图划分为 KNOWN_SHAPE 和 UNKNOWN_SHAPE 集群：

```
DynamicShapePartitioner::Partition()
│
├── MarkUnknownShapeNodes()
│     标记所有包含未知维度（-1）或未知秩（-2）的节点
│
├── InitClusters()
│     为每个节点创建 Cluster
│     类型包括：DATA / KNOWN_SHAPE / UNKNOWN_SHAPE / NETOUTPUT
│
├── MergeClusters()
│     │
│     ├── MergeClustersUnknownShape()
│     │     如果两个 UNKNOWN_SHAPE 集群相连，合并它们
│     │     合并路径上的所有 KNOWN_SHAPE 集群也会被吞并
│     │
│     ├── MergeClustersNormal()
│     │     如果两个 KNOWN_SHAPE 集群之间只有一条路径，合并它们
│     │
│     └── MergeClustersInputData()
│           合并所有 INPUT_DATA 集群
│
└── PruneUniqueClusters()
      去重合并后的集群
```

**合并规则的关键约束**：UNKNOWN_SHAPE 集群具有"传染性"——如果两个未知形状节点之间存在路径，路径上的所有已知形状节点都会被标记为未知形状。这是因为已知形状节点的输出可能作为未知形状节点的输入，需要统一管理。

### 6.2 已知形状图编译

`GraphBuilder::BuildForKnownShapeGraph()` 负责编译 KNOWN_SHAPE 集群：

- 生成完整的 `ModelTaskDef`（包含所有任务的定义）
- 计算精确的内存分配方案（`MemAllocation`）
- 生成零拷贝偏移信息（`ZeroCopyOffset`）
- 输出 `GeModel` 对象，包含编译后的图信息和任务定义

## 7. V2 Runtime Kernel 集成

V2 运行时将静态子图作为 Kernel 集成到执行图中，提供了更细粒度的控制：

### 7.1 DavinciModelCreate

创建并初始化 DavinciModel 实例：

1. 从输入获取 `GeModel` 对象
2. 创建 `DavinciModel` 实例，设置 `known_node_=true`
3. 设置 Session ID、Root Graph ID、Step ID 等上下文信息
4. 初始化权重内存和特征图内存
5. 调用 `DavinciModel::Init()` 完成加载
6. 将 DavinciModel 指针输出到下游 Kernel

### 7.2 DavinciModelUpdateArgs

每次执行前刷新输入输出地址：

1. 从 KernelContext 获取输入输出 Tensor 的设备地址
2. 构造 `vector<uint64_t>` 地址列表
3. 调用 `DavinciModel::UpdateKnownNodeArgs()` 刷新地址

### 7.3 DavinciModelExecute

触发设备侧执行：

1. 先调用 `DavinciModelUpdateArgs` 刷新地址
2. 调用 `rtModelExecute` 触发执行
3. 调用 `CopyOutputData` 拷贝输出数据

### 7.4 DavinciModelUpdateWorkspaces

更新工作内存基址：

1. 从 KernelContext 获取 workspace 地址和内存类型
2. 调用 `DavinciModel::UpdateHbmFmMemBases()` 更新 HBM 内存
3. 调用 `DavinciModel::UpdateExMemBase()` 更新其他类型内存

### 7.5 DavinciModelGetRunAddress

获取运行时内存地址（用于后续 Kernel 的地址依赖）：

1. 根据 `MemoryBaseTypeOffset`（内存类型 + 偏移量）查询实际运行地址
2. 支持 Weight、FileConstant 等内存类型
3. 将地址写入输出 Tensor

## 8. 关键设计决策分析

### 8.1 为什么选择 Task Sink + rtModelExecute 架构？

**替代方案对比**：

| 方案 | Host 侧开销 | 设备侧开销 | 灵活性 |
|------|------------|-----------|--------|
| 每次执行 Kernel Launch | 高（每次都要 Launch） | 低 | 高 |
| Task Sink + rtModelExecute | 低（仅地址刷新） | 低 | 中 |
| 全图编译为单个 Kernel | 最低 | 最低 | 低 |

GE 选择 Task Sink + rtModelExecute 的原因是：

1. **编译期确定性**：KNOWN_SHAPE 子图的所有参数在编译期已知，可以安全地预分发任务
2. **执行效率**：消除 Host 侧 Kernel Launch 开销，rtModelExecute 仅触发预置的任务链
3. **地址刷新灵活性**：通过 `ModelArgsManager` 实现高效的地址刷新，支持动态 I/O 地址

### 8.2 为什么引入 known_node_ 标志而不是创建新类？

在混合执行模式下，`DavinciModel` 通过 `known_node_` 标志切换行为，而非创建独立的子类。这种设计的选择：

**优点**：
- 代码复用最大化：加载、任务分发、执行等核心逻辑完全共享
- 维护成本低：只需在差异点做条件分支
- 内存布局一致：两种模式使用相同的内存管理结构

**代价**：
- 类职责不够单一：一个类同时承担独立模型和子图两种角色
- 条件分支增加复杂度：代码中散布 `if (known_node_)` 判断

代码中的注释也反映了这一点——`UpdateKnownNodeArgs` 方法上的 `// todo 临时方案` 注释表明，未来可能通过重构来改善这一设计。

### 8.3 地址刷新机制的设计权衡

地址刷新机制是静态执行器的核心创新，其设计面临以下权衡：

**全量刷新 vs 增量刷新**：
- 全量刷新：简单可靠，但 H2D 带宽浪费
- 增量刷新：通过 `ret_up` 策略级别控制，只刷新变化的地址，但实现复杂

GE 选择了增量刷新策略，通过 `active_mem_base_id_to_plicy` 映射表记录每个 allocation 的刷新策略级别，在 `UpdateForExecute` 时根据 `ret_up` 决定实际拷贝的数据量。

**零拷贝 vs 中间拷贝**：
- 零拷贝：用户缓冲区直接映射，地址刷新即可，无数据拷贝开销
- 中间拷贝：GE 内部管理 I/O 缓冲区，需要额外的 H2D/D2H 拷贝

GE 优先使用零拷贝模式，仅在用户缓冲区不满足对齐要求或设备不可达时回退到中间拷贝。

## 9. 性能优化要点

### 9.1 Task Sink 预分发

所有 Kernel 任务在模型加载时预分发到设备侧，执行时无需 Kernel Launch。这是静态执行器相比动态执行器最大的性能优势来源。

### 9.2 增量地址刷新

通过 `ModelArgsManager` 的增量刷新策略，最小化每次执行时的 H2D 数据传输量。对于 Frozen Inputs（地址不变的输入），首次执行后不再参与刷新。

### 9.3 零拷贝 I/O

用户提供的设备缓冲区直接映射到 Kernel 参数，避免中间拷贝。在训练场景和大批量推理场景中收益显著。

### 9.4 流复用

通过 `ReusableStreamAllocator` 实现 Stream 复用，减少 Stream 创建和销毁的开销。在多模型并发加载场景下尤为重要。

### 9.5 Shrink 优化

模型加载完成后调用 `Shrink()` 释放 Host 侧的 `GeModel` 对象，减少内存占用。因为所有必要信息已分发到设备侧，Host 侧的图结构不再需要。

## 10. 文件清单

### 运行时核心

| 文件路径 | 功能 |
|----------|------|
| `runtime/v1/graph/load/model_manager/davinci_model.h` | DavinciModel 类定义 |
| `runtime/v1/graph/load/model_manager/davinci_model.cc` | DavinciModel 实现（约 9281 行） |
| `runtime/v1/graph/load/model_manager/model_manager.h` | ModelManager 单例定义 |
| `runtime/v1/graph/load/model_manager/model_manager.cc` | ModelManager 实现 |
| `runtime/v1/graph/load/model_manager/model_args_manager.h` | 参数管理器定义 |

### V2 Kernel

| 文件路径 | 功能 |
|----------|------|
| `runtime/v2/kernel/known_subgraph/davinci_model_kernel.cc` | V2 Kernel 集成：Create/Execute/UpdateArgs |

### 编译器

| 文件路径 | 功能 |
|----------|------|
| `compiler/graph/partition/dynamic_shape_partition.h` | DynamicShapeCluster/Partitioner 定义 |
| `compiler/graph/partition/dynamic_shape_partition.cc` | 已知/未知形状图划分逻辑 |

### 基础接口

| 文件路径 | 功能 |
|----------|------|
| `base/common/model/executor.h` | Executor 抽象接口 |
