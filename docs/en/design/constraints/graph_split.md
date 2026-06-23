# Graph Split Module Constraints Document


Graph execution module should try to avoid dynamic memory allocation (may cause random performance degradation).
Graph compilation module memory reuse processing phase forbids graph modification (multiple reuse algorithms will process concurrently in multiple threads, will cause exceptions).

## 1. Module Positioning and Boundary

Graph split module's responsibility is to perform model splitting by execution semantics on computational graph, clarifying which nodes or subgraphs enter dynamic graph executor, which nodes or subgraphs enter static graph executor, core boundary is "dynamic graph executor / static graph executor" model boundary.

Graph split module only handles splitting itself, not responsible for post-split subgraph internal operator executor selection, kernel selection, stream allocation, memory allocation, GenTask and runtime scheduling strategy, should not perceive other components' internal implementation details, nor should it couple other module logic through cross-component attribute writing.

Design should strictly separate "split decision" and "execution implementation":
- Graph split module responsible for node attribution, path closure, cluster merge and subgraph construction;
- Engine selection, stream allocation, memory reuse, task distribution and other modules each responsible for split results;
- If certain attributes or extension info need to be used both before and after split, must clearly define inheritance and passing rules, avoid boundary fuzziness and responsibility leakage.

## 2. Core Design Principles

1. **Single Responsibility Principle**
   Graph split module only answers "which executor should node/path/cluster enter" question, does not bear executor internal scheduling and resource management responsibilities.

2. **Explicit Rule Principle**
   All dynamic graph split basis must be explicit, interpretable, debuggable rules, forbid introducing inexplicable implicit judgments that cannot be directly interpreted from code and logs.

3. **Topology and Semantic Closure Principle**
   Split results must maintain topology dependency and execution semantic correctness. For static nodes on dynamic path, if their shape, tiling or runtime dependencies cannot be independently established on static side, must be merged into dynamic graph executor, guaranteeing semantic closure.

4. **Minimal Intrusion Principle**
   What can be expressed through cluster internal state, do not additionally write Node/OpDesc attributes; when must write attributes, only allow writing stable attributes necessary for execution boundary determination, and write points must be centralized, traceable, verifiable.

5. **Stability Priority Principle**
   Graph split is pre-stage of stream allocation, memory reuse, GenTask and other subsequent flows, any design modification should prioritize guaranteeing graph structure stability, split result stability and downstream module behavior stability.

6. **Evolvability Principle**
   Rule extension should prefer adopting new independent rules, independent switches, independent attributes methods, avoid directly modifying existing default semantics, reduce impact on old graphs, old configurations and historical debugging methods.

## 3. Dynamic Graph Split Basis

Current dynamic graph split rules are:

1. Operator is dynamic Shape operator (dimension -1 or -2 exists in Shape), goes dynamic graph executor, affects whether whole graph goes dynamic graph split flow;
2. Operator is marked as `force_unknown`, goes dynamic graph executor, affects whether whole graph goes dynamic graph split flow;
3. Operator has Tiling dependency but does not support Tiling sink, goes dynamic graph executor, affects whether whole graph goes dynamic graph split flow;
4. Operator has `_is_support_addr_refresh=false` set, goes dynamic graph executor, affects whether whole graph goes dynamic graph split flow;
5. Operator is on path between two operators already marked as dynamic graph executor operators, needs to be cut to dynamic graph executor, does not affect whether whole graph goes dynamic graph split flow;
6. Operator belongs to HostCpu engine, needs to be cut to dynamic graph executor, does not affect whether whole graph goes dynamic graph split flow;
7. Split static graph subgraph operator count below threshold (threshold configurable through `ge.exec.static_model_ops_lower_limit`), needs to be cut to dynamic graph executor, does not affect whether whole graph goes dynamic graph split flow.

On top of above rules, should also follow these constraints:
- Dynamic path adsorption, small cluster degradation, no-tiling fallback behaviors, essentially belong to joint rules of execution semantic correctness and overall performance balance, not allowed to make isolated judgments only from local node attributes;
- Not allowed to break dynamic chain continuity to preserve local static subgraphs;
- Not allowed to incorrectly expand dynamic graph executor scope to reduce split count, causing unnecessary static capability degradation.

## 4. Multi-threaded Concurrency Scenario Principles

1. **Graph Object Concurrency Model Clarity Principle**
   Graph infrastructure does not support concurrent modification by default, concurrency safety is guaranteed by business layer. Graph split flow should not assume `ComputeGraph`, `Node`, `OpDesc` and other objects have concurrent write capability.

2. **Concurrency Phase Graph Modification Prohibition Principle**
   Graph compilation module forbids graph modification in memory reuse and other multi-threaded concurrent phases, therefore graph split related graph structure modification, attribute writing and subgraph construction must complete before subsequent concurrent phases, not allowed to write back graph structure in memory reuse and other phases.

3. **Shared Resource Protection Principle**
   If adding global cache, rule tables, statistics, registries and other shared resources, must explicitly state thread safety model, do well on shared resource protection. Read-more-write-less scenarios prefer initialization-time construction, runtime read-only methods, avoid introducing lock competition and state drift on graph split hot paths.

4. **Few Attribute Writes Principle**
   In multi-threaded semantic scenarios, try not to write attributes, especially OpDesc attributes. Since attributes are cross-component visible state, forbid spreading attribute writes without clear boundary constraints.

5. **Result Determinism Principle**
   Graph split results should be generated based on topology order, cluster id, explicit sorting and other stable orders as much as possible, avoid depending on non-deterministic container traversal order, guarantee same graph produces consistent split results under same input conditions.

## 5. Debugging and Logging Principles

1. **Boundary Changes Must Have Key Logs**
   Node attribute changes, executor attribution changes, whole graph split decision changes, cluster type changes and other boundary behaviors must have unified and stable key logs.

2. **Key Word Stability Principle**
   Use key word `Mark node` where node attribute changes occur, guarantee consistency with historical scripts, debugging methods and problem localization experience. Existing key log phrasing should remain stable, forbid arbitrarily changing key words and semantics.

3. **Few But Accurate Logs Principle**
   Logs should avoid high-frequency printing, prefer landing on state transition points, rule hit points, degradation points and exception boundary points, content should include node name, node type, trigger reason, key attributes and before/after state, for quick demarcation.

4. **Decision Chain Traceability Principle**
   Debugging should not only see "node becomes dynamic graph/static graph", but also be able to locate specific trigger reason, e.g., dynamic shape, force_unknown, tiling dependency not supporting sink, address refresh not supporting, small cluster degradation, path adsorption etc.

5. **New Rules Synchronously Supplement Observability**
   When adding split rules, cluster types, attribute fields, must synchronously supplement logs, debug output, serialization or other debugging means, avoid problems where functionality is effective but cannot be located.

## 6. Compatibility Modification Principles

1. **External Changes Must Be Reviewed Principle**
   Changes involving external options, environment variables, interfaces, data structures, log phrasing etc., may all affect compatibility, need to go through review and form passing conclusion before implementation.

2. **Prefer Addition Principle**
   Interface and rule evolution prefer adding new interfaces, new attributes, new switches methods, avoid directly modifying existing interface default parameters, existing rule semantics and historical behaviors.

3. **Boundary Stability Principle**
   Once graph split boundary changes, it will link to affect engine split, stream allocation, memory reuse, GenTask, dump/prof, RT2 dynamic flow and many other modules, therefore compatibility analysis must cover entire chain, not just analyze graph split module itself.

4. **Attribute Inheritance Completeness Principle**4. **Attribute Inheritance Completeness Principle**
   If adding ExtAttr or other attributes needed both before and after splitting, must ensure attribute inheritance completeness in subgraph construction, subgraph merging, whole graph restoration flows, avoid attribute loss after graph split, semantic inconsistency issues.

5. **Historical Behavior Retention Principle**
   For existing graph models, existing configurations and production network debugging methods, default should maintain original behavior unchanged; if must adjust, should provide clear switch, migration plan and validation conclusions.

## 7. Performance Principles

1. **Prioritize Equivalent Modification Principle**
   Graph split related requirements and defect fixes should first adopt equivalent modification, ensure original execution semantics, kernel execution paths and overall performance characteristics don't undergo unnecessary changes.

2. **No Kernel Performance Degradation Principle**
   If modification doesn't involve kernel itself, shouldn't downgrade original static execution path to dynamic execution path through split boundary changes, causing kernel-side extra scheduling, synchronization or data搬运 overhead.

3. **Avoid Execution Period Dynamic Memory Allocation Principle**
   Graph execution module should尽量避免 dynamic memory allocation; graph split design should first complete decisions at compile time,不得 introduce random performance degradation through new runtime dynamic resource allocation.

4. **Reduce Fragmented Subgraphs Principle**
   Should avoid generating large amounts of too small static subgraphs or dynamic graph subgraphs. Small cluster downgrade, path adsorption etc. mechanisms fundamentally are to reduce subgraph fragmentation, reduce executor switches, reduce Host/Device roundtrips and synchronization overhead.

5. **Hot Path Lightweight Principle**
   Graph split related hot paths禁止 adding unnecessary logs, timestamp acquisition, repeated calculations and temporary object overhead; performance evaluation should cover split graph duration, subgraph count, cross-subgraph boundary count and subsequent execution chain overhead.

## 8. Typical Constraints and Implementation Requirements

1. Graph split design and implementation should first遵守 module boundaries,不得 expand into "顺便 complete engine selection/stream allocation/resource scheduling" mixed modules.
2. New rules must satisfy "rule interpretable, log observable, behavior verifiable, result reproducible".
3. Any modification involving attribute writes, should verify write scope, write timing and before/after state through UT/ST,必要时 add graph split before/after attribute consistency validation.
4. Any modification involving boundary changes, should supplement whole graph scenario, mixed graph scenario, small cluster scenario, HostCpu scenario, dynamic path adsorption scenario and compatibility scenario validation.
5. If modification will affect subsequent small engine split, stream allocation, memory reuse, RT2 dynamic flow, dump/prof etc. modules, must perform cross analysis and linkage validation, avoid locally correct, whole chain abnormal.

## 9. Graph Split Module Design Review Checklist

#### 1. Module Boundary
- [ ] Whether clearly this modification only acts on "graph split" boundary, doesn't扩散 to executor selection, stream allocation, memory allocation, GenTask, runtime scheduling.
- [ ] Whether clearly new logic belongs to "split graph decision" not "execution implementation".
- [ ] Whether identified and explained affected adjacent modules: small engine split, stream allocation, memory reuse, RT2, dump/prof.
- [ ] Whether defined new attributes or states' ownership and inheritance rules before and after split, before and after subgraph construction.

#### 2. Rule Correctness
- [ ] Whether new rules are explicit, interpretable, debuggable, avoid implicit judgments.
- [ ] Whether explained rule hit input conditions, boundary conditions, exclusion conditions.
- [ ] Whether explained whether this rule affects "whether whole graph enters dynamic graph split flow".
- [ ] Whether explained whether this rule is "node-level to dynamic" or "path/cluster-level to dynamic".
- [ ] Whether verified won't破坏 dynamic link continuity.
- [ ] Whether verified won't错误 expand dynamic graph executor scope.
- [ ] Whether verified won't错误 retain static nodes that should enter dynamic graph executor.

#### 3. Topology and Graph Structure Stability
- [ ] Whether ensure topology dependency semantics unchanged before and after split.
- [ ] Whether ensure cluster merge, downgrade, adsorption won't introduce cycles.
- [ ] Whether ensure graph structure modification timing controlled during graph split.
- [ ] Whether ensure all necessary graph modifications complete before subsequent memory reuse, multithreading phases.
- [ ] Whether evaluated impact on topo sort, topo id, cluster sort stability.
- [ ] Whether avoid relying on unstable container traversal order causing result漂移.

#### 4. Concurrency and Thread Safety
- [ ] Whether clearly data structures this modification involves will be accessed by multithreading.
- [ ] Whether avoid unconstrained writing OpDesc/Node attributes in multithreading semantic scenarios.
- [ ] If introducing global cache, tables, registration info, whether explained thread safety model.
- [ ] If having shared resources, whether having lock, read-only initialization or lifecycle protection design.
- [ ] Whether verified same graph multiple split results consistent under same inputs.

#### 5. Debugging and Logging
- [ ] Whether provided key logs at boundary change points.
- [ ] Whether maintain `Mark node` etc. existing keywords stable.
- [ ] Whether can distinguish specific trigger causes through logs:
  - [ ] dynamic shape
  - [ ] force_unknown
  - [ ] tiling depend doesn't support sink
  - [ ] `_is_support_addr_refresh=false`
  - [ ] path adsorption
  - [ ] small cluster downgrade
  - [ ] HostCpu engine
- [ ] Whether logs avoid high-frequency printing.
- [ ] Whether log content includes node name, type, reason, key attributes, before/after state.
- [ ] Whether new rules or attributes同步补充 debug output or other observability means.

#### 6. Compatibility
- [ ] Whether involves external option, environment variable, interface, data structure, log口径 changes.
- [ ] If involves, whether completed review with clear conclusions.
- [ ] Whether prioritize adopting "new rule/new switch/new attribute" rather than modifying old semantics.
- [ ] Whether evaluated old graph, old configuration, historical scenario compatibility.
- [ ] Whether defined new attributes' inheritance rules in subgraph construction, subgraph merging, whole graph restoration.
- [ ] Whether evaluated compatibility with scripts, debugging, production network troubleshooting methods.

#### 7. Performance
- [ ] Whether遵循 "prioritize equivalent modification".
- [ ] Whether confirmed won't无故 downgrade original static path to dynamic path.
- [ ] Whether confirmed won't introduce kernel performance degradation.
- [ ] Whether avoid new execution period dynamic memory allocation.
- [ ] Whether evaluated impact on subgraph count, cross-subgraph boundary count, sync/搬运次数.
- [ ] Whether evaluated impact on Host/Device roundtrips, scheduling overhead, executor switches.
- [ ] Whether hot paths avoid new unnecessary logs, timestamps, repeated calculations, temporary objects.

#### 8. Test Coverage
- [ ] Whether having UT covering new rule hit scenarios.
- [ ] Whether having UT covering rule miss scenarios.
- [ ] Whether having UT covering boundary scenarios: mixed graph, path adsorption, small cluster, HostCpu, force_unknown, tiling depend, addr refresh.
- [ ] Whether having ST covering linkage scenarios with small engine split, stream allocation, memory reuse, RT2, dump/prof.
- [ ] Whether having before/after attribute consistency validation.
- [ ] Whether having log keyword and debug口径 validation.
- [ ] Whether verified same graph repeated execution results stable.

#### 9. Conclusion Items
- [ ] Rule correct.
- [ ] Boundary clear.
- [ ] Concurrent safe.
- [ ] Debug available.
- [ ] Compatibility acceptable.
- [ ] Performance no degradation.
- [ ] Testing sufficient.
- [ ] Can enter development/can merge.

---