# Static Shape Runtime Constraints Document


**Core Design Philosophy:**
Static shape module is GE runtime's high-frequency execution path, has extremely high performance requirements. All design decisions must take performance as core consideration, ensuring minimized execution overhead while guaranteeing correctness.

**I. Performance Optimization Rules**

1. **Execution Flow Performance Constraints**
   - New requirements involving DavinciModel::NnExecute flow need to evaluate impact of new flow on execution performance, no performance degradation allowed
   - Execution hot path forbids memory allocation, forbids adding unnecessary logs, forbids getting timestamp operations

2. **Memory Allocation Optimization**
   - Should try to avoid dynamic memory allocation during execution, which may cause random performance degradation
   - If memory allocation is necessary, should use pre-allocated memory pools or stack memory

3. **Synchronous Asynchronous Execution Consistency**
   - New copy-type tasks added in execution flow need to maintain consistency with model's synchronous/asynchronous execution mode
   - Judge synchronous/asynchronous based on is_async_mode_ value
   - Asynchronous H2D copy must be paired with HOST_TO_DEVICE_EX option, otherwise may cause host memory premature destruction, leading to issues

**II. Module Responsibility Decoupling Rules**

1. **TaskInfo Responsibility Boundary**
   - TaskInfo only handles task distribution related to this node (task), does not perceive other nodes or model-level handling
   - TaskInfo's responsibility is limited to single node's task construction and distribution, not involving model-level resource management and state maintenance

2. **Inter-module Decoupling**
   - ModelArgsManager responsible for overall planning and allocation of model parameters
   - DavinciModel responsible for model lifecycle management and coordination
   - TaskInfo responsible for specific task construction and execution
   - Each module interacts through clear interfaces, avoid cross-module direct access

**III. ArgsFormat Unified Processing Rules**

1. **ArgsFormat Full Scenario Coverage**
   - When handling operator args, need to add related processing logic for scenarios without argsformat
   - Scenarios without args format supplement default common processing flow, process according to unified args format (refactoring)
   - ArgsFormatInfo provides unified args description and processing capability

**IV. Address Refresh Strategy Rules**

1. **Update Strategy Design**
   - Support multiple update strategies: kNoNeedUpdate, KUpdateHostInput, kUpdateModelIo, kUpdateFmAndModelIo, kInitOneTime
   - Automatically select optimal update strategy based on memory address change situation
   - Support operator-based refresh (UpdateModelParam_static_bin) and traditional H2D copy two methods
   - Support PCIE BAR copy optimization (small data volume scenarios)

2. **Address Refresh Performance Optimization**
   - For frequent refresh scenarios, prefer operator-based refresh method
   - Through UpdateModelParam_static_bin operator batch refresh addresses on Device side, reduce H2D copy count
   - Only refresh changed address segments, avoid full refresh
   - Maintain last_bases_ cache, fast-forward detect address changes

**V. Memory Management Rules**

1. **Memory Type Handling**
   - Support multiple memory types: HBM, TS, HostSVM
   - Use GetRtsMemoryType to get correct memory type
   - TS memory automatically selects optimal TS memory type based on size

2. **Memory Alignment Requirements**
    - All memory allocations must meet alignment requirements (typically 32-byte or 64-byte alignment)
    - host_input_size needs 32-byte alignment
    - args table needs to be allocated by alignment size, ensure access efficiency

3. **Zero Copy Scenario Handling**
    - For zero-copyable inputs/outputs, use zero copy to reduce data transfer
    - For non-zero-copy scenarios, need explicit copy
    - Identify whether supports zero copy through ATTR_IS_ZERO_COPY_BLOCK

**VI. Compile-time Constraint Rules**
1. **Graph compilation phase forbids graph modification**
    - Graph compilation module memory reuse processing phase forbids graph modification
    - Multiple reuse algorithms will process concurrently in multiple threads, graph modification will cause exceptions

2. **ArgsFormat Validation**
    - ArgsFormat needs to be generated at compile time, contain complete tensor description info
    - Runtime strictly follows ArgsFormat description for args assembly
    - Any ArgsFormat changes need full validation

**VII. Debugging and Maintainability Rules**

1. **Key Log Positioning**
    - Functional boundaries need key logs for problem demarcation
    - Log at key positions calling adump interfaces etc.
    - Log content concise and clear, contain necessary context info

2. **DFX Info Statistics**
    - Maintain detailed execution phase time statistics
    - Record address refresh count and latency
    - Support performance analysis and problem localization

**VIII. Compatibility and Stability Rules**

1. **Old Version Compatibility**
    - Virtual memory usage scenarios (rtReserveMemAddress), need compatibility design
    - Old version DRV does not support this interface, need to ensure business flow is normal, no ERROR logs

2. **Stream and Resource Management**
