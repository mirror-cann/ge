# Stream Allocation Constraint Document


**Module Boundaries**

> Static shape: Logic stream allocation -> Memory allocation -> GenTask -> Stream splitting

> Dynamic shape: Logic stream allocation -> GenTask

**Core Design Principles**

1. **Graph Structure Stability Principle**: GE logic stream allocation depends on topo sorting, topo id continuity, graph structure cannot change, otherwise will affect subsequent memory reuse logic and physical stream splitting logic

2. **Stream Allocation Mode Distinction**:
   - **Static shape**: Default multi-stream, supports single-stream special scenarios
   - **Dynamic shape**: Default single-stream; multi-stream is enabled only when `ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1`

3. **Pass Architecture Design**: Stream allocation adopts Pass chain processing architecture, each Pass executes in priority order, each Pass responsible for specific stream allocation rule processing

**Static Shape Logic Stream Allocation**

1. **Stream Allocation Mode**:
   - **Multi-stream mode** (default): Allocate streams at root graph and subgraph granularity, each graph internally allocates streams according to engine subgraphs, same engine theoretically will be allocated to one stream. If node is marked with StreamLabel attribute, will allocate separate stream for it, same StreamLabel will be allocated to same stream
   - **Single-stream mode**: Only allocate one logic stream, if has StreamLabel will report error. Single-stream scenario does not allow StreamLabel

2. **Pass Execution Order** (multi-stream mode):
   - UpdateForMdeGroupPass: Update stream ID according to mde group
   - AssignByLabelPass: Allocate stream according to StreamLabel
   - IndependentStreamPass: Independent engine (such as hccl) needs independent stream
   - AssignByDependencyPass: Allocate stream according to dependency relationship
   - NodeStreamUpdatePass: Assign subgraph stream to node
   - UpdateForParallelGroupPass: Allocate new stream ID for parallel group
   - AllReduceParallelPass: AllReduce and backward operator parallel
   - UpdateForSkippedEnginePass: Optimize stream allocation for skipped engine subgraphs
   - OptimizeIneffectiveMultiStreamPass: Optimize ineffective multi-stream

   Single-stream mode execution order:
   - SingleStreamPass: Single stream allocation
   - NodeStreamUpdatePass: Node stream update
   - UpdateForSkippedEnginePass: Skip engine optimization

3. **Stream Allocation Priority**:
   - User stream label (USER_STREAM_LABEL) highest priority
   - StreamLabel next
   - Engine dependency relationship allocation (when no label)

4. **Stream Reuse Principle**:
   - Independent engine (IsEngineIndependent, such as hccl) does not allow reuse
   - Stream with StreamLabel does not allow reuse
   - Stream reuse needs to satisfy:前后子图scheduler_id相同、前子图后续子图引擎冲突检查、memory_priority模式下SubGraphCouldReuse规则
   - Reuse selection: Select highest priority from reusable subgraph set

5. **Stream Continuity Guarantee**: After stream allocation completion, will refresh stream ID, ensure stream ID starts from 0 continuous allocation. Any Pass that may modify node stream_id (including migrating node to new stream, reordering stream ID, etc.) may cause certain stream_id to no longer have operators and produce holes. Therefore, stream ID continuity refresh must execute **after all Passes that may modify stream_id**, as last step of logic stream allocation phase, can correctly eliminate holes, avoid runtime extra apply physical stream for holes stream_id causing resource waste

6. **Auto Multi-Stream Mode**: Static shape supports enabling automatic multi-stream parallelism through `ge.autoMultistreamParallelMode`, and its existing enable semantics remain unchanged. The DAG multi-stream algorithm is integrated through custom stream pass and must be explicitly configured as `LoadBalance:N` or `MainStream:N`, where `N` is an integer in `[1, 64]`. Compatibility for bare `LoadBalance` defaulting to eight streams has been retired. This Pass may rewrite node stream_id, so it still needs to follow the stream continuity guarantee, ensuring subsequent memory allocation, GenTask and stream splitting see continuous stream IDs

**Static Shape Physical Stream Splitting**

Since physical stream carried task quantity is limited, need to calculate task quantity on same logic stream and do stream splitting, stream and event total number produced in this phase is quantity applied during execution. This phase processes at whole graph granularity.

1. **Stream Activation Relationship Setting**:
   - Set stream activation relationship, subsequent if stream splitting happens, need to update again
   - Here will store streams to be activated with StreamLabel as key
   - **Constraint**: For dynamic gear scenario, when adding StreamLabel need add gear information to distinguish

2. **Event Continuity Guarantee**:
   - Stream allocation internal insert event scenarios mainly include: adjacent different logic stream nodes、stream splitting nodes、main-sub stream、attached stream and main stream
   - **Constraint**: After insert or delete event need consider event id continuity, rts will validate event id. So operate event must uniformly use member Nodes2SyncInfos, stream allocation phase will refresh events in Nodes2SyncInfos, ensure continuity

3. **Stream Splitting Trigger Condition**:
   - Detect whether physical stream supports unlimited depth (FEATURE_TYPE_PERSISTENT_STREAM_UNLIMITED_DEPTH)
   - When supported, RTS automatically handles stream splitting
   - When not supported, split new stream by node task count exceeds threshold
   - NodeSizePerStream threshold: Consider different task upper limits for normal stream and large stream (huge_stream)

**Static Shape Attached Stream Allocation**

1. **Attached Stream Definition**: One node can produce multiple streams, besides main stream has attached stream
2. **Allocation Timing**: Attached stream allocation after main stream allocation completion
3. **Constraint Conditions**:
   - Attached stream allocation only after main stream allocation completion
   - One node maximum supports one attached stream (reuse through reuse_key)
   - Specify attached stream information through ATTR_NAME_ATTACHED_STREAM_INFO or ATTR_NAME_ATTACHED_STREAM_INFO_LIST attribute
   - Supports _old_ and _v2_ two version attribute formats

**Dynamic Shape Logic Stream Allocation**

The default is single-stream. Multi-stream allocation is entered only when `ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1`, and then streams are allocated according to the following rules:

1. **Stream Allocation Strategy**:
   - AssignEnginesOwningStream: Allocate stream according to engine, one engine corresponds one stream_id, limited to AIcoreEngine、VectorEngine、DNN_HCCL、DSAEngine
   - AssignAicpuCanParallel: If ac_parallel_enable is true, then judge whether aicpu and aicore can parallel, if can parallel then allocate separate stream for aicpu (only when input node has no task、only has NoTask input、cannot reuse predecessor subgraph)
   - AssignIndependentAicpuNode: Allocate separate stream for independent aicpu node (such as DROPOUTGENMASK、GETNEXT etc.)
   - AssignWithReuse: Judge whether has reusable subgraph for engine subgraphs not yet allocated stream (both forward and backward can reuse)
   - AssignRemainSubgraphNeedAssignStream: Allocate stream for remaining subgraphs not assigned stream
   - ReassignStreamByStreamLabel: Nodes with stream_label allocated to new stream, same stream_label allocated to same stream (different engines can be allocated to same stream through stream_label)

2. **Stream Reuse Principle**:
    - aicpu cannot attach to hccl (IsEngineIndependent)
    - Engine same or IsEngineAttach can reuse
    - Can pass reuse relationship through reused_subgraph (such as AIcore attach to hccl after, subsequent AIcore can reuse)

3. **Node Stream Constraints**:
   - Specific nodes such as Data, Variable, Constant, ConstPlaceholder, NetOutput and FILECONSTANT are forced to main stream (stream 0)
   - Subgraph nodes must be on main stream
   - Nodes without stream_id are placed on main stream

4. **Stream Continuity**: Refresh stream IDs after sorting by engine priority, ensure stream IDs start from 0 and are continuous, eliminating empty stream references

5. **Auto Multi-Stream Compatibility**: Dynamic Shape multi-stream is enabled only by `ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1`. After the environment switch is enabled, `ge.autoMultistreamParallelMode` selects `cv` or an explicit DAG mode; `cv`, `LoadBalance:N`, and `MainStream:N` cannot enable Dynamic Shape multi-stream by themselves. DAG modes must use `LoadBalance:N` or `MainStream:N`, where `N` is an integer in `[1, 64]`. Compatibility for bare `LoadBalance` defaulting to eight streams has been retired. DAG modes first execute existing rules based on engine reuse and StreamLabel, then run the custom Stream Pass. Because the DAG pass may rewrite stream id, stream IDs need to be made continuous again according to the stream_id actually used by nodes, and the mapping from stream_id to node list needs to be rebuilt. Subsequent Event insertion must be based on this final stream_id to ensure correct cross-stream dependency synchronization

**Constraints and Boundary Conditions**

1. **Static Shape Constraints**:
   - Single-stream mode does not allow StreamLabel
   - Attached stream can only be allocated after main stream allocation completes
   - Event ID must be continuous, uniformly managed through Nodes2SyncInfos
   - StreamLabel in dynamic gear scenarios needs gear information

2. **Dynamic Shape Constraints**:
   - When multi-stream is enabled, ac_parallel_enable value can only be "0", "1" or empty
   - `ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1` must first enable Dynamic Shape multi-stream; only then does `ge.autoMultistreamParallelMode` select the algorithm, and the option cannot replace the environment switch. Explicit `LoadBalance:N`/`MainStream:N` DAG modes must refresh stream IDs to be continuous after all custom Stream Passes that may rewrite node stream_id, while `cv` mode keeps the engine-priority continuity result
   - AICPU engine stream allocation considers parallel scenarios
   - Specific nodes (Data, NetOutput, etc.) must be on main stream
   - Subgraph nodes must be on main stream
