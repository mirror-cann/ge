# Coding Red Lines

## General Rules

### 1. Prohibit Hardcoding Sensitive Information and Using Prohibited Sensitive Words

1. Hardcoding plaintext passwords, keys, and other high-level sensitive data in source code.
2. Having deprecated accounts and passwords left from historical version rectification. In subsequent code delivery like CSEC certification, may be questioned by customers as hidden backdoor accounts, high risk, suggest products completely investigate and thoroughly clean up.
3. Large arrays with unclear purpose or vague data definition, poor functionality readability, easily suspected of having hidden backdoors. "EWA-Canada Security Certification Introduction" also raises such requirements.
4. Hardcoding a public network IP address, domain name, email address, easily questionable sensitive words like backdoor in code or config, can cause questioning of illegally collecting and returning customer data, suspicion of backdoors. Therefore need to investigate each hardcoded public network IP address one by one, confirm if has reasonable explanation. Especially cannot contain IP addresses pointing to Huawei or China.

### 2. External Data as Array Index or Pointer Offset Must Ensure Within Address Size Range

Array index directly controlled by external issue, root cause is internally allocated space insufficient to support external data access. Therefore if memory-related operations accessed by external third party through communication interface, should note if exists scene beyond array, when external data as array index accessing memory, must strictly verify data size.

### 3. Integer Operations Must Be Strictly Checked, Ensure No Overflow, Reversal, Division by 0

Integer issue fundamentally is memory issue. Therefore if integer operation result used in memory-related operations, should note if integer will overflow/reverse.

### 4. After Memory or Handle Pointer-Type Resource Release Should Immediately Assign New Value, Exception Branches Must Synchronously Release, Avoid Causing Resource Leak

Resource leak usually refers to:

1. File operation handles not closed after use
2. Memory not released after use
3. Database etc. not closed after use

Resource leak will cause system resource exhaustion, causing DoS. Such issues mostly occur in exception branches, various conditional branches. During coding, need extra attention.

### 5. Must Check Size Before Memory Allocation, Must Verify Success After Allocation

1. Memory allocation size may come from external data, must check its legality, prevent excessive, illegal memory allocation. Cannot allocate 0-length memory.
2. External allocator needs to verify returned struct pointer and struct internal data pointer both effective.

> **Supplementary Note**: External allocator refers to allocator users can register through interfaces in `ge_api_v2.cc`, no distinction when using, so verification logic needs to cover external allocator return results.

### 6. Don't Write Code Relying on Parameter Evaluation Order (Like func(a++, a)), This Will Cause Undefined Behavior

In C/C++ language, function parameter evaluation order is unspecified, meaning compiler can evaluate parameters in any order. Therefore, code like func(a++, a), since we cannot know which of a++ and a parameters is evaluated first, will cause undefined behavior.

## GE Framework Rules

### 1. Public Interface Reserved Parameters Must Have Strict Verification, Prevent Subsequent Version Enablement Having Compatibility Issues

Reserved parameters need to force users use invalid values, otherwise after subsequent opening for use, previously arbitrarily filled values by users run normally on old version, but abnormally on new version, becoming compatibility issue. For example: pointer-type parameters causing coredump (pointer-type), numeric parameters causing logic errors.

> **Supplementary Note**: Invalid value for numeric reserved parameters is generally 0. Invalid value for pointer reserved parameters is nullptr.

### 2. For Public Interface Return Values or Parameters, Memory Length Data Type Needs to Be Defined as size_t Type

Memory length exceeding 4G scenarios, 32-bit cannot express, using 32-bit data type, encountering corresponding scenario, need to add new interface with same logic, causing extra development and maintenance cost.

### 3. When Modifying Public Interfaces, Must Strictly Ensure API and ABI Compatibility

Header files under `inc/external` directory are public interfaces, need to strictly ensure ABI and API compatibility, if incompatible, will cause customer code modification or recompilation, original program unable to run.

> **Supplementary Note**:
>
> - Header files under `inc/external` directory are all public interface boundaries, other directories considered internal interfaces, can freely modify.
> - C++ public interfaces generally use classes (not structs), when extending add class methods, don't modify existing method signatures.
> - C public structs have reserved fields. When creating new public structs must also reserve fields.
> - In public header file public interfaces prohibit using `std::string` (including function parameters, return values, class/public members etc.), because different C++ standard library/compiler versions `std::string` underlying implementation may differ, causing ABI incompatibility. Should use `AscendString` instead.

### 4. Prohibit Doing Non-Equivalent Transformation When Modifying Graph, Must Consider Control, Data Two Types of Relationship Equivalence

GE's graph optimization normally completely doesn't change graph computation result, when doing such optimization, need to consider "data", "control" relationship simultaneously equivalent.

> **Supplementary Note**:
>
> - Data equivalence means output tensor consistent.
> - Control equivalence means execution order constrained by control edges unchanged.
> - Equivalence judgment scope is **between several modified nodes**, not requiring all nodes' execution order unchanged globally.

### 5. Prohibit Adding Timing-Dependent Pass Without Adding Comments, Must Explain Dependency Content, and File for Approval

Adding timing-dependent pass must be strictly controlled, principle should be no timing dependency, if unavoidable, need to file for approval, centralized management, and add comments explaining as required.

> **Supplementary Note**: Filing for approval has two ways: 1) Apply topic discussion at ge sig regular meeting; 2) Submit issue discussion. Currently no independent timing-dependent pass record document, existing filing information recorded in each pass's code comments.

### 6. Prohibit Hardcoding Chip Type, Frontend Framework Type, Network Type in Code

CANN as heterogeneous computing framework, needs to interface with various frameworks upwards, support multiple hardware and chips downwards, to keep framework able to onetrack evolve, need to decouple from surroundings, not allow sensing chip type, operator type (AICCore operators), frontend framework type.

> **Supplementary Note**: Chip capability difference generally queried through `aclrt` interface, not hardcode chip type judgment. Framework type and network type have no difference for GE, don't need to sense.

### 7. Prohibit Not Considering New Inserted Node and Input/Output Side Nodes' Control Edge Relationship When Inserting Node

When inserting node to graph, need to consider inserted node and front/back nodes' logic relationship, if inserted node has close relationship with input node, need to move input node's output control edge to inserted node, conversely need to move output node's input control forward to inserted node. In actual coding, can also use encapsulated interfaces `GraphUtils::InsertNodeAfter` and `GraphUtils::InsertNodeBefore`.

> **Supplementary Note**: `GraphUtils::InsertNodeAfter` and `GraphUtils::InsertNodeBefore` have completely handled control edge relationship, no unhandled scenarios. Prefer using these two interfaces, after calling no need to manually handle control edges.

### 8. Node Name Unique Constraint Is: Cannot Duplicate

Besides this constraint, theoretically each module, pass can arbitrarily modify node name, delete this node and add another node with same name etc.

> **Supplementary Note**: When needing to locate specific node, should through **traverse graph nodes + match condition** way, condition can be OpType, node attribute etc.

### 9. Prohibit Adding EVENT/TRACE Printing in Load/Execute Flow, Prevent Generating Massive Logs, Must Add Need Filing for Approval

plog directly writes to DPC storage, DPC storage default setting small IO aggregation delay issue, massive logs will cause small IO slow, produce performance fluctuation.

> **Supplementary Note**:
>
> - This restriction only applies to **load/execute flow**, compilation/initialization other flows not restricted.
> - Whether Debug or Release build, load/execute flow **prohibit** adding EVENT/TRACE level logs (default enable will affect performance).
> - **Allow** adding INFO/DEBUG level logs (default disable, after disable won't affect performance) and ERROR level logs.

### 10. When Using rtMemcpy/aclrtMemcpy/rtMemcpyAsync/aclrtMemcpyAsync Need Ensure Operated Memory Effective When Copy Action Happens

rtMemcpy/aclrtMemcpy immediately executes copy action, need ensure operated memory effective when interface called; rtMemcpyAsync/aclrtMemcpyAsync issues to specified stream then immediately returns, delayed scheduling, need ensure operated memory effective when this task actually scheduled to accelerator.

### 11. When Using rtMemcpy/aclrtMemcpy/rtMemcpyAsync/aclrtMemcpyAsync, Need to Judge Source Size>0 Before Copy, Otherwise Copy May Fail

When calling copy, if size is 0, indicates original address is a nullptr, RTS interface will report error.

### 12. When Using rtMemcpyAsync/aclrtMemcpyAsync, H2D Copy Type Needs to Use RT_MEMCPY_HOST_TO_DEVICE_EX/ACL_MEMCPY_HOST_TO_BUF_TO_DEVICE Config Item, Unless Explicitly Know Host Memory to Be Copied Won't Be Released

Difference between with EX and without EX is, with EX option, RTS internally does one h2h copy on host memory, then does async h2d, so upper layer can release its own host memory. Without EX option, underlying won't do h2h copy, if upper layer's host memory released, undefined behavior will occur when actual copy happens. (With EX interface, performance slightly worse)

### 13. When Calling Interfaces Provided by Other Components Requiring GE to Pass Resources, Need Relevant Component to Clarify Whether This Interface Has Lifecycle or Release Timing Constraints on Passed Resources

In solution design also need to clarify whether interfaces with other components have relevant constraints. For example: rtDevBinaryRegister holds pointer passed by GE, and reuses this pointer to do H2D operation when context switch, causing GE must ensure registered binary host memory can only be released after deregistration.

> **Supplementary Note**: Method to identify cross-component interface lifecycle constraints during code review:
>
> - Interfaces starting with `aclrt`: Download [cann/runtime](https://gitcode.com/cann/runtime) code, search documents in `docs/` directory, this directory all are aclrt-starting interface documents.
> - Interfaces starting with `rt`: Find interface prototype in `pkg_inc/runtime/` directory. rt-starting interfaces have no documents, need to read source code to judge, or submit issue in cann/runtime repo to inquire.

### 14. When Doing Graph Add/Delete/Modify Operations, Prohibit Using std::unordered_map Etc. Unordered Containers or Using std::map with Node Pointer as Key for Storage and Traversal, Ensure Multiple Processing Results Consistent

Using std::unordered_map etc. unordered containers, although keys stored inside are consistent, but storage order cannot ensure consistent, when traversing this container to do graph modification processing may cause node insertion order on graph inconsistent, if graph has operators with strict execution order requirements (communication operators), multi-P scenario execution may cause hang issue. Similarly using Node pointer for storage also cannot ensure storage order consistency (for example Node1 and Node2 pointer order may change during multiple executions).

> **Supplementary Note**:
>
> - Recommend using ordered collections (like `std::map`), can use node name as key.
> - Prohibit using `std::unordered_map` etc. unordered containers.
> - Prohibit using Node pointer as key: each process's pointer address differs, causing traversal order inconsistent, multiple execution results may differ.
> - When using node name as key, can only **temporarily store in one local function**, destroy after function ends. Don't cross functions or long-term hold containers with node name as key.

### 15. Prohibit Singleton Mode Implemented Inline in Header File

Singleton mode implemented inline in header file may have these problems:
>
> - Each .cc file (compilation unit) including this header file will generate a local copy of this function, may generate multiple instance issues (core hazard)
> - Although C++ standard specifies static local variable should only initialize once in same program, but in dynamic library (dlopen) scenario, this guarantee may be broken, different SOs (shared libraries) may each hold an instance
> - May appear Coredump caused by dlopen loading order

### 16. Prohibit Using Non-Public RTS Interfaces

For rt-starting interfaces only can use interfaces in pkg_inc/runtime/rt_external*.h header files, interfaces not in these header files prohibited use, need to use aclrt-starting equivalent interfaces.

### 17. Prohibit Doing Cross-SO Function Calls in Static Object/Global Object Destructor

Static objects calling across dynamic libraries (.so), destructor process has several typical risks, root cause is C++ standard doesn't guarantee static object destruction order in different compilation units (including dynamic libraries).

**Risk Types**:

- Undefined destruction order causing dangling reference: Program exit, global/static objects destruct in "construction reverse order", but only valid in same compilation unit. Different .so destruction order decided by linker, unpredictable. If .so A's static object accesses .so B already destructed object during destruction, produces dangling reference causing crash.
- Destructor throwing exception causing program termination: In cross-.so destruction chain, if some object throws exception due to dependency failure and not caught, directly std::terminate().
- Multi-threading race: Worker threads during program exit may access static objects being destructed.
- Dynamic library unload uncertainty: When runtime unloading .so, static object destruction timing uncontrollable.

**Typical Violation Scenarios**:
Static object holding cross-.so object:

```cpp
// Violation: Static singleton destructor, held shared_ptr points to other .so's object
class ExecutorManager {
public:
  static ExecutorManager& GetInstance() {
    static ExecutorManager instance;  // Static object
    return instance;
  }
private:
  std::map<std::string, std::shared_ptr<Executor>> executors_;
  // Executor object in plugin.so, destructor may have .so already unloaded → crash
};
```

**Correct Approach**:
Cleanup work done in Finalize(), destructor doesn't cleanup cross-.so objects:

```cpp
void ExecutorManager::Finalize() {
  executors_.clear();  // Runtime cleanup, .so not unloaded
}

ExecutorManager::~ExecutorManager() = default;  // Destructor executors_ already empty
```

**shared_ptr Trap**:

- shared_ptr destructor calls virtual destructor, virtual destructor defined in object actual type's .so
- If .so already unloaded, calling virtual destructor accesses released memory → crash
- **shared_ptr cannot delay destruction to program exit phase**

**Common False Positives**:

| Scenario | Reason |
|----------|--------|
| void* + mmDlclose | Bare pointer doesn't trigger destructor, mmDlclose is compile link |
| Has Finalize and upper layer calls | Finalize actively cleans up, destructor defensive empty operation |
| runtime calls aclrt* | libascendcl.so compile link, unload order guaranteed |
| Same .so static registration | Same compilation unit, no cross-.so risk |
