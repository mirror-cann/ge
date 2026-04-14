# GE 内存约束文档

# 静态内存复用

**代码位置：** `compiler/graph/build/memory/`

## 约束1：图编译模块内存复用处理阶段禁止改图

精确边界：

- **禁止改图的范围：** `BlockMemAssigner::AssignMemoryWithReuse` 实现及其触发的所有函数
- **多线程入口：** `HybridMemAssigner::Assign` 启动多线程，并发调用AssignMemoryWithReuse
- **明确禁止的：** 对ComputeGraph上的Node增加、修改、删除属性
- **安全的操作：** 读取属性/OpDesc是安全的（系统在遍历时大量读取OpDesc来判断内存分配策略）

## 约束2：动态多batch场景影响分析

- **代码实体：** `DynamicBatchMemAssigner`（`dynamic_batch_mem_assigner.h`）
- **含义：** 系统通过`batch_label_`（由用户或GE上层框架设置）标识不同batch，支持不同batch间的内存复用策略
- **与静态内存复用的矛盾点：**
  - 连续输入的内存在不同batch中会被合并成一个大块进行对齐
  - batch内外没有复用，batch间有对齐策略，导致内存使用效率可能降低
  - 最大拆分大小限制：`kMaxSplitSizeForDynamicBatch = 400MB`（`dynamic_batch_mem_assigner.h`）

## 约束3：静态图内存新增特性需要考虑的场景

| 场景 | 代码标记/依据 | 对复用的影响 |
|------|-------------|-------------|
| **连续内存** | `continuous_block_`（`block_mem_assigner.h`），`ContinuousMemMng`（`continuous_mem.cc`） | 支持连续输入节点的内存合并；不同batch的连续内存可以合并复用 |
| **atomic集中清零** | `atomic_addr_clean_id_`（`block_mem_assigner.h`） | 需要atomic清零的内存块不能被其他节点复用；如果节点没有相关属性则跳过清零 |
| **零拷贝** | `is_zero_copy_`（`block_mem_assigner.h`），`IsNodeAndPeerNodeTaskSupportZeroCopy`（`block_mem_assigner.cc`） | 零拷贝块可跨节点复用（`IsRealSizeReuseBlock`）；零拷贝内存不能合并（多个用户输入地址可能不连续） |
| **不可变地址输出** | `is_fixed_addr_prior_`（`block_mem_assigner.h`） | constant/const/variable/fileconstant/constplaceholder类型的算子output地址在编译期固定，固定地址优先的内存块可被复用但地址不可变 |
| **不支持地址刷新的算子** | HCOM/rtsStreamSwitchByIndex等 | 这些算子的输入输出地址必须稳定，不能使用零拷贝 |
| **P2P内存类型** | `RT_MEMORY_P2P_DDR`（`block_mem_assigner.cc`） | P2P内存不能与其他内存类型合并清零（`graph_mem_assigner.cc`） |

## 约束4：HCOM算子的特殊性

- **"连续"的含义：** 逻辑连续而非物理连续。多个HCOM算子的输出在逻辑上形成连续内存区域，通过`ContinuousMemMng`管理器处理分配和复用。 — `continuous_mem.cc`
- **featureBaseRefreshable配置：**
  - 获取方式：`ge::GetContext().GetOption(ge::OPTION_FEATURE_BASE_REFRESHABLE, refreshable)` — `block_mem_assigner.cc`
  - 成员变量：`is_feature_map_refreshable_`（`block_mem_assigner.h`）
  - 默认值：`false`，配置值为"1"时设为`true`
  - 作用：控制特征图是否可刷新，影响`IsNoNeedAssignMemory`的判断

## 约束5：其他约束

1. **PreAssign/SetOpMemOffset非线程安全：** 只能由单线程调用，其他并发操作需要注意。 — `block_mem_assigner.h`

2. **对齐策略差异：** 零拷贝内存使用32字节对齐，其他使用512字节对齐。 — `graph_mem_assigner.h`

3. **子图NETOUTPUT特殊处理：** 子图中的NETOUTPUT节点不能进行零拷贝。 — `block_mem_assigner.cc`

4. **多batch形状数据节点约束：** 多batch形状数据节点不支持零拷贝。 — `block_mem_assigner.cc`

5. **悬挂内存块管理：** 悬挂的内存块（suspended block）会在下一个节点分配时被释放，生命周期通过`life_time_begin_`和`life_time_end_`管理，一旦设置不能修改。 — `block_mem_assigner.h`

6. **复用策略可配置：** 支持通过`use_range_`、`ascending_sort_`、`reuse_first_release_`、`memory_priority_mode_`等参数动态配置。 — `block_mem_assigner.h`

---

# 动态内存复用

**代码位置：**
- v2层：`runtime/v2/kernel/memory/allocator/`（ScalableAllocator、MemoryPool）
- v1层：`runtime/v1/graph/manager/active_memory_allocator.h`（ActiveMemoryAllocator、ExpandableActiveMemoryAllocator、PhysicalMemoryAllocator）
- 桥接层：`runtime/v2/kernel/memory/device/device_allocator.h`（DeviceAllocator）

## 约束1：ScalableAllocator不支持多线程并发

- **代码位置：** `runtime/v2/kernel/memory/allocator/scalable_allocator.h`
- **无锁设计依据：** 类内部无`std::mutex`或`std::recursive_mutex`，仅有 `static std::atomic_size_t global_allocator_id_` 用于生成唯一ID（`scalable_allocator.h`）
- **安全性保证方式：** 由 `aclmdlExecute` 的调用约束保证单线程调用（详见 `docs/graph_engine_api/aclmdlExecute.md`），底层allocator通过recursive_mutex保证线程安全
- **底层有锁保护：** v1层的PhysicalMemoryAllocator使用 `std::recursive_mutex`（`active_memory_allocator.h`），ExpandableActiveMemoryAllocatorImp同样使用 `std::recursive_mutex`（`active_memory_allocator.h`）

## 约束2：ActiveMemoryAllocator/ExpandableActiveMemoryAllocator/PhysicalMemoryAllocator支持多线程

- **线程安全机制：** 使用 `std::recursive_mutex` 保护共享资源
- **新增代码要求：** 必须在访问共享资源时加锁保护，遵循现有的锁使用模式
---

# 内存管理

**代码位置：** `runtime/v2/kernel/memory/`（不含allocator子目录）

## 约束1：device id正确性

- **代码位置：** `memory_kernel.cc` 使用 `aclrtGetDevice` 获取device_id
- **要求：** 调用rts接口时，device id必须显式传入正确值，避免使用缺省参数（默认为0），需要验证多device场景用例

## 约束2：内存释放时序

- **顺序：** 先流同步 → 再释放内存 → 最后销毁device
- **代码关联：** `caching_mem_allocator.cc`，`AllocateWithTryRecycle`方法确保同步后再释放

## 约束3：虚拟内存兼容性设计

- **rtReserveMemAddress用途：** 虚拟地址预留，用于动态shape预分配地址空间
- **实际调用位置：** `runtime/v1/graph/manager/active_memory_allocator.cc`
- **fallback路径：** 当 `rtReserveMemAddress` 失败时，标记不支持虚拟地址预留，回退到物理地址分配模式。 — `runtime/v1/graph/manager/active_memory_allocator.cc`（"Maybe not support rtReserveMemAddress."）
- **要求：** 要确保业务流程正常，无ERROR日志

## 约束4：caching_mem_allocator进程退出时序

- **代码位置：** `caching_mem_allocator.h` — `static std::vector<CachingMemAllocator *> all_caching_mem_allocators_`
- **机制：** 全局变量保存所有allocator实例指针，GE finalize时主动调用所有allocator的finalize方法
- **原因：** allocator析构时如果还有内存未释放，会调用rts接口释放，但此时rts so可能已卸载，导致core dump
- **finalize入口：** `rts_caching_mem_allocator.cc` 遍历 `all_caching_mem_allocators_` 逐一finalize

## 约束5：单例内存池在模型卸载或者session析构时也要析构
举例 `SessionMemAllocator<ExpandableActiveMemoryAllocator>` 是单例，内部存储基于`session id`的对象。当用户创建新的session，或者调用`aclmdl`开头的接口进行离线推理时会产生新的`session id`，也就会产生新的对象，因此需要在下面两个地方调用`SessionMemAllocator<ExpandableActiveMemoryAllocator>::Instance().RemoveAllocator`释放对象内存，否则用户在创建多个session或者多次执行调用`aclmdl`开头的接口进行离线推理的场景下，无用的对象会多占用host内存或者device内存资源，造成“内存泄漏”现象。
- GeExecutor::UnloadModel `aclmdl`离线推理场景卸载模型
- InnerSession::Finalize InnerSession析构

## CachingMemAllocator完整关系图

```
┌──────────────────────────────────────────────────────────────┐
│  CachingMemAllocator                                         │
│  runtime/v2/kernel/memory/caching_mem_allocator.h            │
│  - all_caching_mem_allocators_ (全局allocator注册表)           │
│  - rts_mem_allocator_ (RtsFirstLevelPool)                    │
│  - memory_pool_ → ScalableAllocator                          │
│  - mutex_ (static, 保护allocator创建/销毁)                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌── RtsCachingMemAllocator                                  │
│  │   runtime/v2/kernel/memory/rts_caching_mem_allocator.h    │
│  │   - device_id_to_allocators_ (device → allocator映射)     │
│  │   - 负责finalize流程，遍历all_caching_mem_allocators_       │
│  │                                                           │
│  └── ScalableAllocator (通过memory_pool_指针)                 │
│       runtime/v2/kernel/memory/allocator/scalable_allocator  │
│       - 无锁设计，单线程调用                                    │
│       - 通过 DeviceAllocator 桥接到 v1 allocator               │
│                                                              │
│       └── DeviceAllocator                                    │
│            runtime/v2/kernel/memory/device/device_allocator   │
│            - active_memory_allocator_ (Expandable...Imp, v1)  │
│                                                              │
│            └── PhysicalMemoryAllocator (v1)                  │
│                 runtime/v1/graph/manager/active_mem...h      │
│                 - recursive_mutex 保护                       │
│                 - 最终调用 rtMalloc / rtFree                  │
└──────────────────────────────────────────────────────────────┘
```

# 编译期内存排布冲突

**代码位置：** `compiler/graph/optimize/mem_layout_conflict_optimize/`

## 维测日志约束
- 发现冲突：`[MemConflict][Conflict] type: [%s, %s], anchor: [%s, %s], will insert %s %s`
- 插入Identity：`[MemConflict][INSERT][NODE]`
- 日志使用`[MemConflict]`关键字，如果每张图只会打印一次，使用`LOGI`

# 跨模块约束汇总

以下约束跨越多个模块，修改时需同步考虑：

1. **地址刷新能力（模块一 + 模块二）：** mem_layout_conflict_optimize中的冲突检测判定"不支持地址刷新"的算子列表，必须与静态内存复用中的处理逻辑保持一致。

2. **v1/v2 allocator边界（模块三 + 模块四）：** v2的ScalableAllocator通过DeviceAllocator桥接到v1的PhysicalMemoryAllocator。修改任何一层的接口都需要考虑对另一层的影响。

3. **caching_mem_allocator生命周期（模块三 + 模块四）：** CachingMemAllocator的finalize流程依赖 `all_caching_mem_allocators_` 全局注册表，新增allocator类型必须正确注册到此表中。
