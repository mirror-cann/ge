# ES Module Ownership Relationship Reversal Analysis

## Problem Description

The ownership relationship between Python layer and C++ layer is **reversed**:

### C++ Layer Ownership Relationship

```cpp
struct EsCGraphBuilder {
    std::list<std::unique_ptr<ResourceHolder>> resource_holder_;  // Owns all resources
    std::unique_ptr<ge::Graph> graph_;
    // ...
};

struct EsCTensorHolder {
    EsCGraphBuilder &owner_graph_builder_;  // Just a reference, does not own
    ge::GNode producer_;
    int32_t producer_out_index_;
};
```

**Relationship**: `EsCGraphBuilder` **owns** `EsCTensorHolder`

### Python Layer Ownership Relationship

```python
class GraphBuilder:
    _handle: EsCGraphBuilderPtr  # C object pointer (does not own)
    
class TensorHolder:
    _handle: EsCTensorHolderPtr  # C object pointer (does not own)
    _builder: GraphBuilder        # Strong reference, owns
```

**Relationship**: `TensorHolder` **owns** `GraphBuilder`

## Why Such Design?

### Reason: Prevent Dangling Pointers

```python
def create_tensor():
    builder = GraphBuilder("my_graph")
    tensor = builder.create_const_float(1.0)
    return tensor  # builder will be GC

# Problem scenario
t = create_tensor()
# If TensorHolder does not hold GraphBuilder:
# - builder has been GC, Python object destroyed
# - builder.__del__() calls EsDestroyGraphBuilder()
# - underlying C++ EsCGraphBuilder is destructed
# - underlying C++ EsCTensorHolder is also released (because owned by GraphBuilder)
# - t._handle is now a dangling pointer!

# Current design (TensorHolder holds GraphBuilder):
# - builder Python object is GC, but because t._builder holds reference, won't actually destruct
# - underlying C++ objects remain valid
# - t._handle still valid 
```

## Potential Problem Analysis

### Problem 1: Circular Reference Risk

#### Scenario Description

```python
class GraphBuilder:
    def __init__(self):
        self._tensors = []  # If saved tensor list
    
    def create_const_float(self, value):
        tensor = TensorHolder._create_from(handle, self)
        self._tensors.append(tensor)  # ⚠️ Circular reference!
        return tensor

# Circular reference:
# GraphBuilder._tensors -> TensorHolder
# TensorHolder._builder -> GraphBuilder
```

#### Impact

- Python GC cannot automatically reclaim (needs to wait for cycle detection)
- May cause memory leak
- Object destruction delay

#### Solution

```python
# Solution 1: Do not save TensorHolder in GraphBuilder
class GraphBuilder:
    # ❌ Don't do this
    # self._tensors = []
    pass

# Solution 2: Use weak reference
import weakrefclass GraphBuilder:
    def __init__(self):
        self._tensors = []  # Save weak references
    
    def create_const_float(self, value):
        tensor = TensorHolder._create_from(handle, self)
        self._tensors.append(weakref.ref(tensor))  # Weak reference
        return tensor
```

**Current code status**: ✅ Safe, GraphBuilder does not save TensorHolder list

---

### Problem 2: Semantic Inconsistency

#### C++ Layer Expectation

```cpp
{
    EsCGraphBuilder builder("my_graph");
    auto tensor1 = builder.CreateConstFloat(1.0);
    auto tensor2 = builder.CreateConstFloat(2.0);
    // tensor1, tensor2 are raw pointers, lifecycle managed by builder
} // builder destructs, all tensors also released
```

#### Python Layer Actual Behavior

```python
def test():
    builder = GraphBuilder("my_graph")
    tensor1 = builder.create_const_float(1.0)
    return tensor1

t = test()  # builder out of scope
# ✅ tensor1 still valid (because holding builder)
# ⚠️ But this is different from C++ semantics!
```

#### Impact

- **API semantic confusion**: C++ and Python behavior inconsistent
- **User confusion**: Users familiar with C++ may misunderstand Python behavior
- **Documentation burden**: Need extra explanation of differences

#### Is This Really a Problem?

**This approach has no problem**
Python and C++ lifecycle management are inherently different:
- Python: Reference counting + GC
- C++: RAII + manual management

**Pythonic approach**: Objects should remain valid as long as referenced

---

### Problem 3: Multi-Builder Scenario Limitations

#### Scenario: Cross-Builder Tensor Usage

```python
builder1 = GraphBuilder("graph1")
builder2 = GraphBuilder("graph2")

tensor1 = builder1.create_const_float(1.0)

# ❌ Should not be allowed theoretically
result = builder2_some_op(tensor1)  # tensor1 belongs to builder1

# But since tensor1._builder is builder1
# Newly generated tensor will also associate to builder1
# Leading to logical confusion
```

#### Impact

- Cross-Builder operations may cause underlying graph structure confusion
- Hard to detect and report errors
- May cause C++ layer assertion failures

#### Solution

```python
# Check builder consistency during operations
def add(self, other: 'TensorHolder') -> 'TensorHolder':
    if not isinstance(other, TensorHolder):
        raise TypeError("Operand must be a TensorHolder")
    
    # Check if from same builder
    if self._builder is not other._builder:
        raise ValueError("Cannot operate on tensors from different GraphBuilders")
    
    # ... subsequent logic
```

**Current code status**: Checks have been added

---

### Problem 4: State Management After build_and_reset()

#### Scenario: Continue Using Builder After Build

```python
builder = GraphBuilder("my_graph")
tensor1 = builder.create_const_float(1.0)
builder.set_graph_output(tensor1, 0)

graph = builder.build_and_reset()  # Build complete

# ⚠️ Can we continue using builder?
tensor2 = builder.create_const_float(2.0)  # Not allowed

# ⚠️ Can we continue using tensor1?
result = tensor1 + tensor2  # tensor1's builder already in built state
```

#### C++ Layer Implementation

```cpp
std::unique_ptr<ge::Graph> BuildGraphAndReset() {
    // ...
    return std::move(graph_);  // Graph object transferred!
}
```

**Problem**: After `build_and_reset()`, `graph_` becomes nullptr, GraphBuilder becomes "empty shell"

#### Impact

- Builder's state unclear after `build_and_reset()`
- Continued use may cause undefined behavior
- Old tensor references builder that's already "invalid"

#### Solution

```python
class GraphBuilder:
    def __init__(self):
        self._is_built = False
    
    def build_and_reset(self):
        if self._is_built:
            raise RuntimeError("GraphBuilder has already been built")
        
        graph_ptr = esb_lib.EsBuildGraphAndReset(self._handle)
        self._is_built = True  # Mark as built
        return Graph._create_from(graph_ptr)
    
    def create_const_float(self, value):
        if self._is_built:
            raise RuntimeError("Cannot create tensors after graph has been built")
        # ...
```

**Current code status**: Checks have been added

---

### Problem 5: Graph Object Ownership Management Conflict

#### Scenario Description

Python's `Graph` object and C++'s `ge::Graph*` have **opposite ownership semantics** in different usage scenarios, causing resource management conflicts.

#### Ownership Contradiction in Two Usage Scenarios

**Scenario 1: GraphBuilder.build_and_reset() Return**

```python
builder = GraphBuilder("my_graph")
x = builder.create_input(0)
builder.set_graph_output(x, 0)
graph = builder.build_and_reset()
# At this point: Python's graph object owns C++ Graph* ownership
```

C++ side implementation:
```cpp
EsCGraph *EsBuildGraphAndReset(EsCGraphBuilder *builder) {
    return static_cast<EsCGraph *>(
        static_cast<void *>(builder->BuildGraphAndReset().release())  // release() transfers ownership
    );
}
```

**Ownership**: Python owns, Python responsible for release ✅

---

**Scenario 2: Graph Passed as Subgraph Parameter**

```python
sub_graph = create_subgraph()  # Python owns ownership

main_builder = GraphBuilder()
result = If(condition=..., then_graph=sub_graph, ...)
# Problem: sub_graph's C++ resource taken over by C++ side
```

C++ side implementation:
```cpp
Esphony_IfOutput Esphony_If(..., EsCGraph *then_branch, ...) {
    auto &builder = ...->GetOwnerBuilder();
    
    // AddResource takes ownership of then_branch
    auto then_ptr = builder.AddResource(
        std::unique_ptr<ge::Graph>(then_branch)  // C++ takes ownership
    );
    // ...
}
```

**Ownership**: C++ owns, C++ responsible for release

**Conflict**: If Python also tries to release → Double free!

#### Specific Problems

**Problem 5.1: Double Free**

```python
branch_graph = create_subgraph()
result = If(..., then_graph=branch_graph, ...)

# Problem:
# 1. C++ side took ownership of branch_graph via AddResource
# 2. Python's branch_graph.__del__() will also call DestroyGraph()
# → Double free!
```

**Problem 5.2: Subgraph Python Object Becomes Dangling Reference**

```python
sub_graph = create_subgraph()

main_builder = GraphBuilder()
result = If(..., then_graph=sub_graph, ...)
# sub_graph ownership transferred

final_graph = main_builder.build_and_reset()
del main_builder  # Explicit del or out of scope, main_builder gets GC

#  sub_graph._handle points to freed memory
print(sub_graph.name)  # Accessing wild pointer!
```

#### Problem Root Cause

**Scenario-Dependent Ownership Semantics**:

| Scenario | Should Python release? | Should C++ release? | Expected behavior |
|----------|----------------------|--------------------|-----------------|
| build_and_reset() return | ✅ Yes | ❌ No | Python owns alone |
| Passed as subgraph | ❌ No | ✅ Yes | C++ owns alone |

But `Graph` class's `__del__()` cannot distinguish these two scenarios!

```python
class Graph:
    def __del__(self):
        # ❌ Problem: Doesn't know which scenario
        # Scenario 1: Should release
        # Scenario 2: Should not release
        destroy_graph(self._handle)  # May cause double free!
```

#### Solution

**Introduce Ownership Marking Mechanism**

```python
class Graph:
    def __init__(self, name="graph"):
        self._handle = create_graph(...)
        self._owns_handle = True   # ✅ Ownership mark
        self._owner = None         # ✅ Ownership taker reference
    
    def __del__(self):
        # ✅ Decide whether to release based on ownership mark
        if self._owns_handle:
            destroy_graph(self._handle)
    
    def _transfer_ownership_when_pass_as_subgraph(self, new_owner: GraphBuilder):
        """Transfer ownership to C++ side
        
        Args:
            new_owner: GraphBuilder that takes ownership,
                      keeps reference to prevent it being GC prematurely
        """
        self._owns_handle = False    # Python no longer releases
        self._owner = new_owner      # Keep reference, prevent new_owner GC
```

**Automated Processing: Code Generator Inserts Ownership Transfer**

```cpp
// py_generator_utils.h - GenSubgraphConversion()
static void GenSubgraphConversion(...) {
    // Generate: subgraph._transfer_ownership_when_pass_as_subgraph(owner_graph_builder)
    ss << subgraph_name << "._transfer_ownership_when_pass_as_subgraph(" 
       << "owner_graph_builder" << ")\n";
}
```

Generated Python code:
```python
def If(..., then_graph, else_graph, ...):
    owner_graph_builder = ...
    
    # ✅ Auto-generated: Transfer ownership
    then_graph._transfer_ownership_when_pass_as_subgraph(owner_graph_builder)
    else_graph._transfer_ownership_when_pass_as_subgraph(owner_graph_builder)
    
    result = c_lib.EsphonyIf(...)
    return result
```

#### Problem Resolution Effect

**Problem 5.1 - Double Free**: ✅ Resolved
- `_transfer_ownership_when_pass_as_subgraph()` sets `_owns_handle=False`
- Python's `__del__()` no longer releases resources
- Only C++ side releases

**Problem 5.2 - Subgraph Dangling Reference**: ✅ Resolved
- `sub_graph._owner` holds reference to `main_builder`
- As long as `sub_graph` exists, `main_builder` won't be GC
- As long as `main_builder` exists, C++ resources remain valid

#### Reference Chain Protection Mechanism

```python
sub_graph = create_subgraph()
result = If(..., then_graph=sub_graph, ...)
```
```
Python Reference Chain:

    sub_graph (Graph)
        │
        └─ _owner ──────────┐
                            ↓
    result (TensorHolder)   main_builder (GraphBuilder)
        │                       │
        └─ _builder ────────────┘
                                └─ _handle → EsCGraphBuilder
                                              └─ resource_holder_
                                                   └─ [sub_graph's C++ Graph*]

Guarantees:
1. result exists → main_builder exists → C++ resources valid
2. sub_graph exists → main_builder exists → C++ resources valid
```

**Current code status**: ✅ Implemented and tested

---