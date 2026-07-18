# 流分配约束文档


**模块边界**

> 静态shape：逻辑流分配->内存分配->GenTask->流拆分

> 动态shape：逻辑流分配->GenTask

**核心设计原则**

1. **图结构稳定性原则**：GE逻辑流分流时依赖topo排序、topo id连续，图结构不能发生变化，否则会影响后续内存复用逻辑以及物理流拆分逻辑

2. **分流模式区分**：
   - **静态shape**：默认多流，支持单流特殊场景
   - **动态shape**：默认单流，仅当 `ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1` 时开启多流

3. **Pass架构设计**：流分配采用Pass链式处理架构，各Pass按优先级依次执行，每个Pass负责特定分流规则的处理

**静态shape逻辑流分配**

1. **分流模式**：
   - **多流模式**（默认）：以根图、子图粒度分流，每个图内部根据引擎子图进行流分配，相同引擎的理论上会分到一条流上。如果节点被打上了StreamLabel属性，则会为其单独分流，相同StreamLabel的会分到同一条流
   - **单流模式**：只会分配一条逻辑流，如果有StreamLabel则会报错。单流场景下不允许有StreamLabel

2. **Pass执行顺序**（多流模式）：
   - UpdateForMdeGroupPass：根据mde group更新流ID
   - AssignByLabelPass：根据StreamLabel分配流
   - IndependentStreamPass：独立引擎（如hccl）需要独立流
   - AssignByDependencyPass：根据依赖关系分配流
   - NodeStreamUpdatePass：将子图流分配到节点
   - UpdateForParallelGroupPass：为parallel group分配新的流ID
   - AllReduceParallelPass：AllReduce和backward算子并行
   - UpdateForSkippedEnginePass：优化跳过引擎子图的流分配
   - OptimizeIneffectiveMultiStreamPass：优化无效的多流

   单流模式执行顺序：
   - SingleStreamPass：单流分配
   - NodeStreamUpdatePass：节点流更新
   - UpdateForSkippedEnginePass：跳过引擎优化

3. **分流优先级**：
   - 用户流标签（USER_STREAM_LABEL）优先级最高
   - StreamLabel次之
   - 引擎依赖关系分配（无标签时）

4. **流复用原则**：
   - 独立引擎（IsEngineIndependent，如hccl）不允许复用
   - 带StreamLabel的流不允许复用
   - 流复用需满足：前后子图scheduler_id相同、前子图后续子图引擎冲突检查、memory_priority模式下的SubGraphCouldReuse规则
   - 复用选择：从可复用子图集中选择优先级最高的

5. **流连续性保证**：流分配完成后，会刷新流ID，确保流ID从0开始连续分配。任何可能修改节点 stream_id 的 Pass（包括迁移节点到新流、重排流ID等）都可能导致某些 stream_id 上不再有算子而产生空洞。因此，流ID连续性刷新必须在**所有可能修改 stream_id 的 Pass 之后**执行，作为逻辑流分配阶段的最后一步，才能正确消除空洞，避免运行时为空洞的 stream_id 额外申请物理流造成资源浪费

6. **Auto Multi-Stream 模式**：静态shape支持通过 `ge.autoMultistreamParallelMode` 开启自动多流并行，既有开启语义不变。DAG多流算法通过自定义stream pass接入，须显式配置为 `LoadBalance:N` 或 `MainStream:N`，`N` 为 `[1, 64]` 范围内的整数；裸 `LoadBalance` 默认8流的兼容配置已下线。该 Pass 可能改写节点 stream_id，因此仍需遵守流连续性保证，确保后续内存分配、GenTask 和流拆分看到的是连续流 ID

**静态shape物理流拆分**

由于物理流承载的task数量是有限的，所以要对同一个逻辑流上的task计算数量并且做流拆分，这个阶段产生的stream和event总数就是在执行时申请的数量。该阶段以整图粒度做处理。

1. **流的激活关系设置**：
   - 设置流的激活关系，后续如果发生流拆分，还需要再更新一下
   - 这里会以StreamLabel为key存储要被激活的流
   - **约束**：对于分档场景，添加StreamLabel时需要加上档位信息以区分

2. **Event连续性保证**：
   - 流分配内部插入event的场景主要有：相邻的不同逻辑流节点之间、流拆分的节点之间、主从流之间、附属流与主流之间
   - **约束**：插入或者删除event后需要考虑event id的连续性，rts会对event id做校验。所以操作event必须统一使用成员Nodes2SyncInfos，流分配阶段会刷新Nodes2SyncInfos里的event，保证连续性

3. **流拆分触发条件**：
   - 检测物理流是否支持无限深度（FEATURE_TYPE_PERSISTENT_STREAM_UNLIMITED_DEPTH）
   - 支持时，由RTS自动处理流拆分
   - 不支持时，按节点task数超阈值拆分新流
   - NodeSizePerStream阈值：考虑普通流和大流（huge_stream）的不同task上限

**静态shape附着流分配**

1. **附着流定义**：一个节点可以产生多个流，除主流外还有附着流
2. **分配时机**：在主流分配完成后进行附着流分配
3. **约束条件**：
   - 主流分配完成后才能分配附着流
   - 一个节点最多支持一个附着流（通过reuse_key复用）
   - 通过ATTR_NAME_ATTACHED_STREAM_INFO或ATTR_NAME_ATTACHED_STREAM_INFO_LIST属性指定附着流信息
   - 支持_old_和_v2_两种版本的属性格式

**动态shape逻辑流分配**

默认是单流，仅当 `ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1` 时才进入多流分配，并根据以下规则分流：

1. **分流策略**：
   - AssignEnginesOwningStream：根据引擎分流，一个引擎对应一个stream_id，仅限于AIcoreEngine、VectorEngine、DNN_HCCL、DSAEngine
   - AssignAicpuCanParallel：如果ac_parallel_enable为true，则判断aicpu和aicore是否能并行，如果能并行则为aicpu单独分流（仅输入节点无任务、仅有NoTask输入、不能复用前驱子图时分配）
   - AssignIndependentAicpuNode：为独立aicpu节点单独分流（如DROPOUTGENMASK、GETNEXT等）
   - AssignWithReuse：对还没分流的引擎子图判断是否有可复用的子图（前后向均可复用）
   - AssignRemainSubgraphNeedAssignStream：为剩余未分配流的子图分配流
   - ReassignStreamByStreamLabel：带stream_label的节点分到新流，相同stream_label的分到同一条流（不同引擎可通过stream_label分配到同一条流）

2. **流复用原则**：
   - aicpu不能附加到hccl（IsEngineIndependent）
   - 引擎相同或IsEngineAttach时可复用
   - 可通过reused_subgraph传递复用关系（如AIcore附加到hccl后，后续AIcore可复用）

3. **节点流约束**：
   - Data、Variable、Constant、ConstPlaceholder、NetOutput、FILECONSTANT等特定节点强制在主流（stream 0）
   - 子图节点必须在主流
   - 无stream_id的节点放到主流

4. **流连续性**：按引擎优先级排序后刷新流ID，保证流ID从0开始连续，消除空的流引用

5. **Auto Multi-Stream 兼容**：动态shape多流仅由 `ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1` 开启。环境变量开关开启后，`ge.autoMultistreamParallelMode` 才用于选择 `cv` 或显式 DAG 模式；`cv`、`LoadBalance:N` 和 `MainStream:N` 不能单独开启动态shape多流。DAG 模式须配置为 `LoadBalance:N` 或 `MainStream:N`，`N` 为 `[1, 64]` 范围内的整数；裸 `LoadBalance` 默认8流的兼容配置已下线。DAG 模式会先执行按引擎复用和 StreamLabel 的既有规则，再运行自定义 Stream Pass。由于 DAG pass 会改写 stream id，需要按节点实际使用的 stream_id 重新连续化，并重建 stream_id 到节点列表的映射。后续 Event 插入必须基于该最终 stream_id，保证跨流依赖同步正确

**同步机制设计**

1. **EventType类型**：
   - kEvent：普通event
   - kNotify：同步流间的notify

2. **插入场景**：
   - 相邻的不同逻辑流节点之间
   - 流拆分的节点之间
   - 主从流之间（StreamActive和被激活流）
   - 附着流与主流之间（根据ATTR_NAME_ATTACHED_STREAM_INFO的依赖关系）
   - **子图入口边界**：子图内无前驱的入口节点与父节点的输入源节点之间，若处于不同流则必须插入event。任何修改stream_id的Pass（如CustomStreamPass）都可能导致子图入口节点与父节点输入源被分到不同流上，缺少同步event会导致子图在输入数据未就绪时提前启动，引发精度问题

3. **优化策略**：
   - OptimizeBySendEvents：根据流内节点顺序删除冗余send event
   - OptimizeByRecvEvents：根据流内节点顺序删除冗余recv event
   - OptimizeByStreamActivate：通过StreamActive优化跨流event（被激活流与激活流之间不需要event）
   - Event复用：多档位场景构建event重用映射，多维度场景event复用

**流激活机制**

1. **ActiveLabelList处理**：
   - 设置节点的active_label_list属性
   - 建立label到流的映射（labeled_streams_）
   - 记录specific_activated_labels_和specific_activated_streams_

2. **流激活流程**：
   - SetActiveStreamsByLabel：根据active_label_list设置流激活关系
   - SetActiveStreamsForSubgraphs：处理While/For循环子图的首节点激活
   - UpdateActiveStreamsForSwitchNode：更新Switch节点的激活流列表
   - UpdateActiveStreamsForLoop：设置循环场景的流激活

3. **约束**：
   - 循环场景下的流激活需要正确配置
   - 子图首节点StreamActive需要激活同子图内其他流

**约束和边界条件**

1. **静态shape约束**：
   - 单流模式不允许有StreamLabel
   - 主流分配完成后才能分配附着流
   - Event ID必须连续，通过Nodes2SyncInfos统一管理
   - 分档场景StreamLabel需要加档位信息
   - 打了`ATTR_NAME_RTS_LABEL_NODE`属性的节点必须分配在默认流上（根图主流0，或子图父节点所在的流），不能跟随子图的stream_id。该约束在NodeStreamUpdatePass中执行，确保控制流标签节点（如LabelSet、LabelGotoEx、LabelSwitchByIndex等）与其父节点在同一流上执行，避免跨流控制流语义错误

2. **动态shape约束**：
   - 多流开启时，ac_parallel_enable值只能是"0"、"1"、空
   - 必须先设置 `ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1` 开启动态shape多流，之后 `ge.autoMultistreamParallelMode` 才选择算法；该 option 不能代替环境变量开关。显式 `LoadBalance:N`/`MainStream:N` DAG 模式必须在所有可能改写节点 stream_id 的自定义 Stream Pass 之后重新连续化，`cv` 模式保持引擎优先级连续化结果
   - AICPU引擎的流分配考虑并行场景
   - 特定节点（Data、NetOutput等）必须在主流
   - 子图节点必须在主流

3. **通用约束**：
   - 图结构在流分配期间不能改变
   - Topo ID必须连续
   - StreamAllocator支持多线程，但shared resource需要保护
   - ScalableAllocator不支持多线程并发（无锁设计）
   - 逻辑流分配阶段禁止改图，避免影响内存复用和物理流拆分

4. **动态图特殊处理**：
   - 动态图上的While算子的静态body子图的NetOutput需要在stream id 0上，避免多流构图bug
   - 动态图需要考虑多batch场景的影响

---
