## Dump模块整体设计文档

---

### 模块概述

#### 模块边界

Dump模块负责在模型执行过程中将算子输入/输出张量数据、溢出信息、异常上下文等关键数据落盘，用于精度调优、问题定位和性能分析。其边界包括：

- **功能范围**：支持同步/异步dump、溢出检测dump、异常dump；覆盖静态shape和动态shape图；支持单算子、单流、多流、多线程场景。
- **交互组件**：与GE执行引擎、内存管理、流管理、HCCL、RTS（Runtime System）、acl接口、GE Option、环境变量等存在耦合。
- **不负责范围**：不负责数据解析（由离线工具完成）；不干预模型正常执行逻辑。

#### 架构分层

Dump模块采用分层设计，按执行流程分为：

```text
┌─────────────────────────────────────────────────────────────┐
│                    入口配置层                                  │
│  DumpManager - 全局单例，管理多session的DumpProperties          │
│  DumpProperties - 存储dump配置（路径、模式、黑名单、过滤列表）  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    图执行适配层                                │
│  静态图(RT1.x): DataDumper - davinci_model集成                │
│  动态图(RT2.0): ExecutorDumper - hybrid/rt2 executor集成       │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    核心操作层                                  │
│  DumpOp - 构建OpMappingInfo proto，启动aicpu dump kernel      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    底层落盘层                                  │
│  adump interface - 调用RTS提供的底层接口完成实际数据落盘        │
└─────────────────────────────────────────────────────────────┘
```

---

### 核心设计思想

1. **逻辑复用与差异化并存**：dump、溢出检测、异常dump底层复用同一套数据流转和落盘框架，但需根据触发条件、数据类型、处理优先级进行差异化处理。

2. **动态适配性**：支持运行时动态开关dump功能，约束条件可实时刷新，避免重启或重构图。

3. **入口一致性**：环境变量、GE Option、acl接口三种开启方式必须行为一致，相互影响需显式定义。

4. **性能无损**：dump开启时不应导致执行性能显著劣化（尤其RT2.0多流多线程场景），禁止在热路径增加非必要内存分配或同步操作。

5. **可定界性**：提供关键日志和接口调用轨迹，便于快速定界问题是dump模块自身缺陷还是上层使用问题。

---

### 详细设计

#### 1. 模块职责划分

| 模块            | 职责                                           | 位置                                             |
|-----------------|------------------------------------------------|--------------------------------------------------|
| `DumpManager`   | 全局单例，管理多session的dump配置，提供动态开关能力 | `common/dump/dump_manager.h`                      |
| `DumpProperties`| 存储单个session的dump配置（路径、dump模式、黑名单、算子过滤列表等） | `common/dump/dump_properties.h` |
| `DumpOp`        | 构建OpMappingInfo proto，分配device内存，启动aicpu dump kernel完成dump | `base/common/dump/dump_op.{h,cc}` |
| `DataDumper`    | RT1.x（静态图）集成，遍历所有需要dump的算子，收集地址信息，生成OpMappingInfo交给adump | `runtime/v1/common/dump/data_dumper.{h,cc}` |
| `ExecutorDumper`| RT2.0（动态shape）集成，基于Subscriber机制，在节点执行前后触发dump | `runtime/v2/subscriber/dumper/executor_dumper.{h,cc}` |
| `ExceptionDumper`| 异常场景dump，保存异常现场上下文                    | `common/dump/exception_dumper.h`                 |

#### 2. 静态图(RT1.x)处理流程 (`davinci_model.cc` + `data_dumper.cc`)

在静态图加载阶段，`DavinciModel`完成以下dump相关处理：

```cpp
// 在模型加载过程中
1. 从DumpManager获取当前session的DumpProperties
2. 判断是否需要dump，如果需要则创建DataDumper实例
3. 遍历模型中所有节点，对需要dump的节点调用SaveDumpTask()保存dump信息
4. LoadDumpInfo()调用rtDatadumpInfoLoad将dump信息下发到device
5. 模型执行完成后，UnloadDumpInfo()清理资源
```

**关键设计点**：
- HCCL算子处理：HCCL在静态图中按照动态逻辑处理，需要特殊处理输入输出地址获取。`DataDumper`通过`ATTR_DATA_DUMP_REF`属性支持重定向到原始算子地址。
- L1/L1Fusion地址处理：对于L1内存上的张量，跳过直接dump，仅在需要时生成OpBuffer。
- 黑名单过滤：支持按算子名、算子类型黑名单过滤，减少不必要dump。

#### 3. 动态图(RT2.0)处理流程 (`executor_dumper.cc`)

RT2.0采用Subscriber机制，`ExecutorDumper`作为订阅者在执行事件点触发dump：

```text
┌──────────────────┐
│  ModelStart      │ → Init dumper, Update step num, Reset FSM
├──────────────────┤
│  ExecuteStart    │ → Set FSM state, HCCL特殊处理前插入dump
├──────────────────┤
│  ExecuteEnd      │ → Fill dump info, DoDataDump, Check overflow
├──────────────────┤
│  ModelEnd        │ → Count iteration, Clear dump debug resources
└──────────────────┘
```

**关键设计点**：
- 依赖更新机制：由于动态shape地址在运行时才能确定，`ExecutorDumper`需要依赖前驱节点更新完成后才能获取正确地址。通过`kernel_idxes_to_dump_units_`维护依赖关系，等待所有依赖更新完成后才执行dump。
- FFTSPlus支持：对于FFTSPlus场景，通过`ffts_dump_op_`保存子算子信息，在加载时设置到任务info中。
- HCCL特殊处理：HCCL通信算子需要在执行前后分别dump input和output，`InsertHcclDumpOp()`在`ExecuteStart`dump input，在`ExecuteEnd`dump output。
- 溢出检测：在执行结束后通过`rtStreamSynchronizeWithTimeout`检测是否发生溢出，如果检测到溢出则触发溢出dump。

#### 4. `DumpOp`核心流程 (`dump_op.cc`)

`DumpOp`是dump操作的核心执行器，负责构建proto和启动dump kernel：

```text
LaunchDumpOp()
  ├─ Set dump path, step, model name
  ├─ Get task_id and stream_id from RTS
  ├─ Create Task proto
  ├─ LaunchDump() → DumpInput + DumpOutput based on dump mode
  │    ├─ 黑名单过滤
  │    ├─ 获取地址，填充shape/dtype/format信息
  │    ├─ 添加到Task proto
  │    └─ 添加到OpMappingInfo
  └─ ExecutorDumpOp()
       ├─ Serialize OpMappingInfo to string
       ├─ Allocate device memory for proto and proto size
       ├─ H2D memcpy
       └─ Launch aicpu kernel "DumpDataInfo"
```

**关键设计点**：
- 动态地址更新：支持运行时更新输入输出地址（`UpdateAddrs()`），用于动态shape场景。
- FFTSPlus支持：`GenerateFftsDump()`专门处理FFTSPlus多context场景，为每个context生成单独task。
- 循环信息传递：支持将global_step、loop_per_iter、loop_cond地址传递给dump kernel，实现按step/dump。

#### 5. `DataDumper`核心设计 (`data_dumper.cc`)

`DataDumper`负责静态图场景下组织所有dump信息：

- **OpMappingInfo构建**：将所有需要dump的算子整理成`OpMappingInfo` protobuf，包括task、input/output、shape、地址等信息。
- **地址重定向**：通过`ATTR_DATA_DUMP_REF`支持将dump请求重定向到其他节点的输入输出，解决AIPP等场景下的地址获取问题。
- **JSON shape解析**：对于FFTSPlus切片信息，通过JSON解析获取动态shape，计算正确的tensor size。
- **L1fusion特殊处理**：对L1内存上的张量跳过直接dump，通过`OpBuffer`机制处理。

#### 6. HCCL差异化处理

静态图和动态图对HCCL处理方式不同：

| 场景     | 处理方式                                                                 |
|----------|--------------------------------------------------------------------------|
| 静态图   | 通过`ATTR_DATA_DUMP_REF`重定向到原始地址，依赖图构建时已经保存了映射       |
| 动态图   | 在`ExecuteStart`触发input dump，在`ExecuteEnd`触发output dump，保证获取正确的通信后数据 |

---

### 功能入口与配置优先级

#### 开启方式

Dump支持三种开启方式：

1. **环境变量**：
   - `DUMP_GE_PATH`：dump输出路径
   - `DUMP_GE_MODE`：dump模式（input/output/all）
   - `DUMP_GE_LAYER`：指定需要dump的算子列表

2. **GE Option**：
   - 通过`ge::SetGlobalOption`设置dump相关选项

3. **ACL接口**：
   - `aclmdlSetDumpConfig`：动态设置dump配置
   - 支持运行时动态开关

#### 优先级规则

```text
acl接口 > 环境变量 > GE Option
```

- 若acl接口显式设置了dump配置，则忽略GE Option和环境变量
- 当多个入口同时提供配置时，采用"非空覆盖"策略——后加载的配置仅覆盖已显式设置的项，未设置的项保留之前的值
- 若环境变量和GE Option同时配置且内容冲突（例如dump路径不同），打印ERROR日志并选择高优先级入口的配置

---

### 多场景支持

#### 普通数据dump

- **触发时机**：每个迭代执行过程中
- **数据内容**：算子指定的input/output张量
- **落盘方式**：aicpu kernel异步落盘

#### 溢出检测dump

- **触发条件**：`rtStreamSynchronizeWithTimeout`返回溢出错误
- **优先级**：高于普通dump
- **设计要点**：
  - 仅在op debug模式开启后才能使用
  - 检测到溢出后设置need_overflow_dump标志，触发dump
  - 支持AiCore、AiCpu、FFTSPlus等多种kernel类型

#### 异常dump

- **触发条件**：执行异常时
- **数据内容**：除了张量数据，还包括tiling_data、args、workspace等上下文信息
- **设计要点**：
  - 通过`ExceptionDumper`统一保存
  - 支持Normal和FFTSPlus两种处理器分别处理
  - 异常发生后保存现场不影响模型继续退出

---

### RT2.0适配要点

#### 设计挑战

RT2.0（也叫Davinci 2.0）引入了动态shape、多流、多线程执行模型，对dump模块带来以下挑战：

1. **地址动态性**：输出地址在执行前无法确定，必须在执行后才能获取
2. **并行执行**：多流多线程并发，需要保证线程安全
3. **子图嵌套**：控制流（While/If/Case）导致子图嵌套，需要正确处理依赖关系
4. **FFTSPlus**：动态切片带来多context，每个context需要单独dump

#### 解决方案

1. **依赖更新机制**：`NodeDumpUnit`维护`total_update_count`和`cur_update_count`，等待所有依赖更新完成后才执行dump

   ```cpp
   if (++dump_unit->cur_update_count != dump_unit->total_update_count) {
     continue; // 等待所有依赖更新完成
   }
   ```

2. **按节点维护dump unit**：每个node对应一个`NodeDumpUnit`，存储已更新的input/output地址和shape
3. **控制流特殊处理**：在`InitOrderHoldersFromControlNodes()`中特殊处理While/If子图中的exit节点，正确建立依赖关系
4. **FFTSPlus多context支持**：保存每个context的input/output地址，单独生成dump任务

#### 性能考虑

- **热路径无分配**：`ExecutorDumper`在初始化阶段预分配所有`NodeDumpUnit`，执行阶段不动态分配
- **惰性地址拷贝**：仅对host上的tensor执行H2D拷贝，device上的tensor直接使用原生地址
- **最小同步**：仅在dump完成后执行一次流同步，不影响其他算子执行

---

### HCCL处理要点

#### 问题背景

在静态图中，HCCL算子被特殊处理，其输入输出地址在图编译时无法确定，需要在运行时从通信对端获取。dump需要特殊处理才能正确获取地址。

#### 解决方案

**静态图**：
- 通过`ATTR_DATA_DUMP_ORIGIN_OP_NAMES`和`ATTR_DATA_DUMP_REF`属性保存原始算子引用
- dump时根据引用找到实际地址

**动态图**：
- HCCL算子需要在执行前dump input，执行后dump output
- `InsertHcclDumpOp()`分别在`ExecuteStart`和`ExecuteEnd`事件触发dump
- 保证input在通信前dump，output在通信完成后dump

---

### 动态开关支持

#### 设计要点

- `GlobalDumper`支持注册回调，当dump开关状态变化时通知所有`ExecutorDumper`
- 对于包含静态子图的模型（RT2.0中的`DavinciModelExecute`），支持动态加载/卸载dump信息：

  ```cpp
  void LoadDumpTaskForDavinciModels(bool dump_enable) {
    for (auto davinci_model : davinci_models) {
      dump_enable ? davinci_model->ReLoadDumpInfo() : davinci_model->UnloadDumpInfo();
    }
  }
  ```

#### 约束

- 配置变化在下一个迭代生效，当前迭代正在执行的任务不受影响
- 动态开关不影响已经在device上的dump信息，需要重新加载才能生效

---

### 日志与可定界性

#### 日志规范

- **入口日志**：调用任何dump接口（如`aclmdlSetDumpConfig`）时，打印包含调用栈、参数摘要的INFO日志，关键字为`dumper`
- **状态变更日志**：dump开关状态变化、配置刷新、环形缓冲区创建/销毁等事件，打印INFO日志
- **错误分级**：
  - 用户配置错误（如路径不可写）→ WARNING，提示正确配置方法
  - dump内部错误（如内存分配失败）→ ERROR，并自动降级（如丢弃部分数据继续执行）
  - 数据损坏或RTS接口返回异常→ ERROR，并触发异常dump保存现场

#### 文件名约定

每个dump文件名称包含：

```text
[场景]_[模型名]_[算子名]_[算子类型]_[迭代号]_[流ID]_[任务ID]
```

便于快速定位问题。

---

### 约束与限制

1. **RT2.0不支持watcher模式**：动态shape op当前不支持watcher mode，会输出警告日志跳过

2. **L1内存不直接dump**：L1/L1Fusion上的张量需要通过特定方式处理，不支持直接dump地址

3. **单算子场景特殊处理**：单算子dump不需要设置step信息，走特殊路径

4. **空shape过滤**：shape为空的可选输出会被跳过dump，减少无用数据

---

### 关键设计原则回顾

1. **逻辑复用**：dump、溢出检测、异常dump复用`DumpOp`和`DataDumper`核心逻辑，仅在触发时机和优先级上区分

2. **入口一致性**：三种入口最终都解析为`DumpProperties`，核心逻辑不区分入口来源

3. **性能优先**：
   - 禁止在热路径动态分配内存
   - 最小化同步操作，仅在dump完成后同步一次
   - 黑名单过滤尽早跳过不需要dump的算子

4. **可调试性**：
   - 关键路径都有INFO级别日志，包含算子名、索引、地址等信息
   - 错误信息包含足够的上下文用于定位

5. **线程安全**：RT2.0多线程场景下，每个`ExecutorDumper`维护自身状态，无共享可变状态

---

### 相关文件

| 文件                                       | 说明                    |
|--------------------------------------------|-------------------------|
| `base/common/dump/dump_op.{h,cc}`          | DumpOp核心实现          |
| `runtime/v1/common/dump/data_dumper.{h,cc}`| 静态图(RT1.x)dump实现   |
| `runtime/v2/subscriber/dumper/executor_dumper.{h,cc}` | 动态图(RT2.0)dump实现 |
| `runtime/v1/graph/load/model_manager/davinci_model.cc` | 静态图模型加载集成dump |
| `common/dump/dump_manager.{h,cc}`          | 全局dump配置管理        |
| `common/dump/dump_properties.{h,cc}`       | dump配置存储            |
| `common/dump/exception_dumper.{h,cc}`      | 异常dump实现            |

---
