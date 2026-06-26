# Passes

## Product Support Status

| Product | Support Status |
| :----------------------------------------------------------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.passes import (
    FusionBasePass,
    PatternFusionPass,
    DecomposePass,
    PassStage,
    pattern,
    register_fusion_pass,
    register_decompose_pass,
    create_pattern,
    create_replacement,
)
```

## Functionality Description

Passes module provides Python-level custom graph fusion Pass development framework. Users define graph optimization Passes by inheriting `FusionBasePass`, `PatternFusionPass` or `DecomposePass`, and register them into GE compilation flow through registration decorators.

- `FusionBasePass`: Fusion Pass base class, users need to implement `run()` method.
- `PatternFusionPass`: Pattern matching based fusion Pass, inherits from `FusionBasePass`, users can define patterns through `patterns()` or `@pattern` methods, and implement `replacement()`; `meet_requirements()` is optional implementation. Its `run()` method will not be called by engine, should not be overridden.
- `DecomposePass`: Operator decomposition Pass, inherits from `FusionBasePass`, users need to implement `meet_requirements()` and `replacement()` methods. Its `run()` method will not be called by engine, should not be overridden.

---

## PassStage Enumeration

Pass execution stage enumeration, used to specify Pass registration timing in GE compilation flow.

### Enumeration Values

| Enumeration Value | Description |
| :----------------------------------------------------------- | :----------------------------------------------------------- |
| `BEFORE_INFER_SHAPE` | Before shape inference |
| `AFTER_INFER_SHAPE` | After shape inference |
| `AFTER_BUILTIN_FUSION_PASS` | After built-in fusion Pass |
| `AFTER_ORIGIN_GRAPH_OPTIMIZE` | After original graph optimization |

---

## FusionBasePass Base Class

Base class for all custom fusion Passes.

### Function Prototype

```python
class FusionBasePass:
    def run(self, graph: Graph, context: PassContext) -> StatusLike:
        ...
```

### Parameter Description

| Parameter | Input/Output | Description |
| :------ | :-------- | :---- |
| graph | Input | Computation graph object to be optimized, type is `ge.graph.Graph`. |
| context | Input | Pass execution context, type is `PassContext`, provides current compilation environment information. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `StatusLike` | Returns `None`, `bool` or `int`. Returning `None` or truthy value means execution success, returning falsy value (`False` or `0`) means execution failure. |

---

## PatternFusionPass Base Class

Pattern matching based fusion Pass, inherits from `FusionBasePass`. Execution engine will call `patterns()`, `meet_requirements()` and `replacement()` three hook methods, instead of `run()` method.

### Constraint Description

- **Must not override `run()` method**: If `run()` method is defined in subclass, will throw `TypeError` at class definition time.
- Must implement `patterns()` or at least one `@pattern` method, and implement `replacement()` method; `meet_requirements()` is optional implementation (default returns `True`).
- `@pattern` method cannot be used together with `patterns()` method.
- Does not support `patterns(self, inputs)` style; for expression-style patterns please use `@pattern` method.

### patterns() Method

Defines list of patterns to be matched. This method is legacy explicit graph construction entry, suitable for directly returning one or more `Pattern` / `Graph` objects.

#### Function Prototype

```python
def patterns(self) -> Iterable[PatternOrGraph]:
    ...
```

#### Parameter Description
No parameters.

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `Iterable[PatternOrGraph]` | Returns an iterable object, where each element is of `Pattern` or `Graph` type, representing the subgraph pattern to be matched. |

---

## PassContext Class

Compilation Pass context, Python view of C++ side ``CustomPassContext``. Injected by engine into ``FusionBasePass.run(graph, context)`` for querying or setting Pass name, error message and compilation options.

### Constraint Description

- Only can be used within current ``run`` call stack (or other engine-defined synchronous callbacks), do not save to ``self`` for use by other threads.
- ``get_option_value(key)``: When ``key`` is illegal or underlying call does not return ``GRAPH_SUCCESS``, will throw ``RuntimeError``.

### Method Description

| Method | Description |
| :--- | :--- |
| `get_pass_name() -> str` | Returns current Pass name. |
| `get_error_message() -> str` | Returns error message set by engine or Pass (returns empty string if not set). |
| `set_error_message(message: str) -> None` | Sets error message for GE to record or report. |
| `set_pass_name(name: str) -> None` | Sets Pass name (usually managed by engine, generally no need to call). |
| `get_option_value(key: str) -> str` | Returns compilation option string value corresponding to ``key``. ``key`` must exist and be readable, otherwise throws ``RuntimeError``. |

### Example

```python
from ge.passes import FusionBasePass, PassContext


class MyPass(FusionBasePass):
    def run(self, graph, context: PassContext):
        name = context.get_pass_name()
        opt = context.get_option_value("some.option.key")
        # self._ok is user-defined graph validation method
        if not self._ok(graph):
            context.set_error_message("my pass: invariant violated")
            return False
        return True
```

For more usage, refer to https://gitcode.com/cann/ge/tree/master/examples/fusion_pass .

---

### @pattern Method

Use Python expression to define a pattern. One `@pattern` method corresponds to one pattern, multiple patterns can be declared via multiple `@pattern` methods.

#### Function Prototype

```python
@pattern
def add_zero(self, inputs):
    return inputs[0] + 0
```

#### Parameter Description

| Parameter Name | Input/Output | Description |
| :----- | :-------- | :--- |
| inputs | Input | Expression-style pattern input collection. `inputs[i]` represents the `i`-th graph input; `inputs[:N]` is used to explicitly declare multiple consecutive graph inputs. |

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `TensorHolder` | Returns single-output pattern expression. |
| `list[TensorHolder]` / `tuple[TensorHolder, ...]` | Returns a multi-output pattern; this list or tuple does not represent multiple patterns. |

#### Constraint Description

- `@pattern` method only declares pattern, does not receive `match_result`.
- Multi-pattern pass is declared through multiple `@pattern` methods, not through one method returning multiple `Pattern` / `Graph`.
- `inputs` has unknown input count, therefore cannot be directly iterated; for multi-input scenarios use `inputs[:N]`.
- Python layer automatically creates `GraphBuilder`, graph inputs, graph outputs, and automatically captures visited `inputs` and pattern outputs returned by `@pattern`.
- Auto-capture order is fixed: first capture visited `inputs` by input index order, then capture pattern outputs by `return` structure order. When the same Tensor serves as both input and output, it will be captured once for each role.

### meet_requirements() Method

Determine whether match result satisfies replacement condition.

#### Function Prototype

```python
def meet_requirements(self, match_result: MatchResult) -> bool:
    ...
```

#### Parameter Description

| Parameter Name | Input/Output | Description |
| :---------- | :-------- | :---- |
| match_result | Input | Pattern match result, type is `MatchResult`, contains matched nodes and edges information. |

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `bool` | Returns `True` to indicate replacement condition is satisfied, will execute replacement; returns `False` to indicate not satisfied, skip this replacement. Default returns `True`. |

### replacement() Method

Generate replacement subgraph.

#### Function Prototype

```python
def replacement(self, match_result: MatchResult) -> Graph:
    ...
```

Expression-style pattern can also use the following style:

```python
def replacement(self, inputs) -> TensorHolder:
    ...

def replacement(self, inputs, match_result) -> TensorHolder:
    ...
```

#### Parameter Description

| Parameter Name | Input/Output | Description |
| :---------- | :-------- | :---- |
| match_result | Input | Pattern match result, type is `MatchResult`, contains matched nodes and edges information. |

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `Graph` | Returns replaced subgraph, type is `ge.graph.Graph`. |

Expression-style `replacement(self, inputs)` can return `TensorHolder` or non-empty `TensorHolder` list / tuple, Python layer will automatically construct replacement graph; when need to read match details, can add `match_result` parameter.

---

## PatternMatcherConfig Class

Pattern matcher behavior configuration, used to control constant value matching, IR attribute matching and other matching behaviors. Constructed through `PatternMatcherConfigBuilder`, and passed into `PatternFusionPass` constructor.

### Method Description

| Method | Description |
| :--- | :--- |
| `is_enable_const_value_match() -> bool` | Returns whether constant value matching is enabled. |
| `is_enable_ir_attr_match() -> bool` | Returns whether IR attribute matching is enabled. |

---

## PatternMatcherConfigBuilder Class

Fluent builder for `PatternMatcherConfig`, generates configuration instance through chain calls to configure matching options then calling `build()`.

### Method Description

| Method | Description |
| :--- | :--- |
| `__init__() -> None` | Creates an empty builder. |
| `enable_const_value_match() -> PatternMatcherConfigBuilder` | Enables constant value matching, returns ``self`` to support chain calls. |
| `enable_ir_attr_match() -> PatternMatcherConfigBuilder` | Enables IR attribute matching, returns ``self`` to support chain calls. |
| `build() -> PatternMatcherConfig` | Constructs and returns ``PatternMatcherConfig`` instance. If C++ side construction fails, will throw ``RuntimeError``. |

### Example

```python
from ge.passes import PatternFusionPass, PatternMatcherConfigBuilder


class TestPass(PatternFusionPass):
    def __init__(self):
        super().__init__(
            PatternMatcherConfigBuilder()
            .enable_const_value_match()
            .enable_ir_attr_match()
            .build()
        )
```

For more usage, refer to https://gitcode.com/cann/ge/tree/master/examples/fusion_pass .

---

## pattern Decorator

Marks a method of `PatternFusionPass` as expression-style pattern declaration.

### Function Prototype

```python
def pattern(method: Callable[..., object]) -> Callable[..., object]:
    ...
```

### Parameter Description

| Parameter Name | Input/Output | Description |
| :----- | :-------- | :--- |
| method | Input | Instance method in `PatternFusionPass` subclass, signature should be `method(self, inputs)`. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `Callable[..., object]` | Returns original method object, and collected as pattern by `PatternFusionPass` at class definition stage. |

### Example

```python
from ge.passes import PatternFusionPass, pattern


class AlgebraicPass(PatternFusionPass):
    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0

    @pattern
    def mul_one(self, inputs):
        return inputs[0] * 1

    def replacement(self, inputs):
        return inputs[0]
```

---

## DecomposePass Base Class

Operator decomposition Pass, inherits from `FusionBasePass`. Execution engine will call `meet_requirements()` and `replacement()` methods for matched nodes, instead of `run()` method.

### Constraint Description

- **Must not override `run()` method**: If `run()` method is defined in subclass, will throw `TypeError` at class definition time.
- Must implement `replacement()` method, `meet_requirements()` is optional implementation (default returns `True`).
- Subclass can define class attribute `op_types: Optional[List[str]]` to specify operator type list to be decomposed. When using `register_decompose_pass` decorator, this attribute will be automatically set.

### meet_requirements() Method

Determine whether node needs decomposition.

#### Function Prototype

```python
def meet_requirements(self, node: Node) -> bool:
    ...
```

#### Parameter Description

| Parameter Name | Input/Output | Description |
| :---- | :-------- | :---- |
| node | Input | Node to be judged, type is `ge.graph.Node`. |

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `bool` | Returns `True` to indicate needs decomposition, will execute replacement; returns `False` to indicate doesn't need decomposition, skip. Default returns `True`. |

### replacement() Method

Generate decomposition subgraph.

#### Function Prototype

```python
def replacement(self, node: Node) -> Graph:
    ...
```

#### Parameter Description

| Parameter Name | Input/Output | Description |
| :---- | :-------- | :---- |
| node | Input | Node to be decomposed, type is `ge.graph.Node`. |

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `Graph` | Returns decomposed subgraph, type is `ge.graph.Graph`. |

---

## register_fusion_pass Decorator

Class decorator for registering fusion Pass, used to register `FusionBasePass` or `PatternFusionPass` subclass into GE compilation flow.

### Function Prototype

```python
def register_fusion_pass(*, name: str, stage: PassStage, kind: Optional[str] = None) -> callable:
    ...
```
### Parameter Description

| Parameter Name | Input/Output | Description |
| :----- | :-------- | :---- |
| name | Input | Pass name, string type, must be unique, cannot duplicate with registered Pass names. |
| stage | Input | Pass execution stage, type is `PassStage` enumeration. |
| kind | Input | Pass type identifier, optional parameter. If not specified, when decorated class is `PatternFusionPass` subclass, will automatically set to `"pattern_fusion"`, otherwise set to `"fusion_base"`. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `callable` | Returns class decorator function, decorated class will be registered into Pass registry, and attached with `__ge_pass_descriptor__` attribute. |

---

## register_decompose_pass Decorator

Class decorator for registering decomposition Pass, used to register `DecomposePass` subclass into GE compilation flow.

### Function Prototype

```python
def register_decompose_pass(*, name: str, stage: PassStage, op_types: Iterable[str]) -> callable:
    ...
```

### Parameter Description

| Parameter Name | Input/Output | Description |
| :------- | :-------- | :---- |
| name | Input | Pass name, string type, must be unique, cannot duplicate with registered Pass names. |
| stage | Input | Pass execution stage, type is `PassStage` enumeration. |
| op_types | Input | Operator type list to be decomposed, type is string iterable object, cannot be empty, and each element must be non-empty string. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `callable` | Returns class decorator function, decorated class will be registered into Pass registry, meanwhile `op_types` will be set as class attribute. |

---

## create_pattern Function

Build native Pattern object from pattern graph.

### Function Prototype

```python
def create_pattern(graph: Graph) -> Pattern:
    ...
```

### Parameter Description

| Parameter Name | Input/Output | Description |
| :---- | :-------- | :---- |
| graph | Input | Pattern graph, type is `ge.graph.Graph`. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `Pattern` | Returns constructed native `Pattern` object. |

---

## Pattern Class

Pattern graph wrapper, used to match subgraph in target graph. Usually constructed via `create_pattern`.

### Constraint Description

- ``__init__(graph)`` will **move** ``graph`` out from Python wrapper and make it invalid, after construction don't continue using that ``Graph`` variable.
- ``release()`` is GE bridge layer internal method, Python Pass developers don't need to call directly. Framework will automatically call ``release()`` after ``patterns()`` returns ``Pattern`` to hand native ``Pattern*`` to C++ side pipeline.
- ``capture_tensor`` accepts ``TensorHolder`` / ``Node`` with ``index``, or a ``NodeIo`` style object (with ``node`` / ``index`` attributes), in this case ``index`` parameter should be left empty (``None``).

### Pattern.__init__ Constructor Method

#### Function Prototype

```python
class Pattern:
    def __init__(self, graph: Graph) -> None:
        ...
```

#### Parameter Description

| Parameter Name | Input/Output | Description |
| :---- | :-------- | :---- |
| graph | Input | Pattern graph, type is ``ge.graph.Graph``. After construction this ``graph`` handle will be invalidated. |

### Pattern.capture_tensor Method

Mark Tensor to be captured in pattern graph. Captured Tensors will be saved in call order, subsequently can be read via `match_result.get_captured_tensor(index)`.

#### Function Prototype

```python
class Pattern:
    def capture_tensor(self, source: Union[TensorHolder, Node, NodeIo], index: Optional[int] = None) -> Pattern:
        ...
```

#### Parameter Description

| Parameter Name | Input/Output | Description |
| :----- | :-------- | :---- |
| source | Input | Tensor source to be captured. Supports `TensorHolder`, `Node` or `NodeIo`. |
| index | Input | Output index. When `source` is `TensorHolder` or `Node` can specify output index, default is `0` if not specified; when `source` is `NodeIo` don't need to pass. |

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `Pattern` | Returns current `Pattern`, supports chain calls. |

### Pattern.get_captured_tensors Method

Return captured Tensor list.

#### Function Prototype

```python
class Pattern:
    def get_captured_tensors(self) -> list[NodeIo]:
        ...
```

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `list[NodeIo]` | Returns captured Tensor list in ``NodeIo`` form (contains ``node`` and ``index``). |

### Pattern.is_valid Method

Determine whether current object still holds unreleased native ``Pattern``.

#### Function Prototype

```python
class Pattern:
    def is_valid(self) -> bool:
        ...
```

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `bool` | Returns ``True`` to indicate still holds unreleased native ``Pattern``. |

### Pattern.release Method

Release Python side ownership, and return C++ ``Pattern*`` handle as integer.

#### Function Prototype

```python
class Pattern:
    def release(self) -> int:
        ...
```

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `int` | Returns integer value corresponding to C++ ``Pattern*`` handle. |

#### Constraint Description

- This method is for GE bridge layer internal use, Python Pass developers don't need to call directly. Framework will automatically call ``release()`` after ``patterns()`` returns ``Pattern``.
- Successful ``release`` can only be called at most once per instance.

### Example

```python
# builder is pattern graph builder, add, matmul are nodes already built in pattern graph
pat = create_pattern(builder.build_and_reset([add]))
pat.capture_tensor(matmul)
pat.capture_tensor(add)
```

For more usage, refer to https://gitcode.com/cann/ge/tree/master/examples/fusion_pass .---

## MatchResult Class

Result of one pattern matching, borrowed from C++ side ``MatchResult``, its lifecycle is held by engine/bridge layer. Used as input parameter in ``PatternFusionPass.meet_requirements`` and ``replacement``.

### Constraint Description

- Don't keep ``MatchResult`` beyond current ``meet_requirements`` / ``replacement`` call.
- After bridge layer calls ``_invalidate()`` in ``finally``, subsequent method calls may fail.
- ``get_captured_tensor(i)``: ``i`` must be consistent with ``capture_tensor`` order, otherwise C++ layer will fail and throw exception.
- ``_invalidate()`` is only for GE bridge layer to call, don't call in Pass code.

### Method Description

| Method | Description |
| :--- | :--- |
| `get_matched_nodes() -> list[Node]` | Returns matched ``Node`` instance list in pattern order. |
| `get_captured_tensor(capture_index: int) -> NodeIo` | Returns the ``capture_index``-th captured Tensor (``NodeIo``) in ``capture_tensor`` order. |
| `get_pattern_graph_name() -> str` | Returns pattern graph name. |
| `__str__() -> str` | Returns human-readable debug string (from C++ ``ToAscendString``). |

### Example

```python
# match_result is input parameter of replacement(self, match_result) or
# meet_requirements(self, match_result)
nodes = match_result.get_matched_nodes()
tensor0 = match_result.get_captured_tensor(0)  # NodeIo
n0, idx0 = tensor0.node, tensor0.index
```

For more usage, refer to https://gitcode.com/cann/ge/tree/master/examples/fusion_pass .

---

## NodeIo Class

Describes a node output anchor, contains ``node`` and ``index`` two attributes. Used as optional input type in ``Pattern.capture_tensor``, used as return value type in ``MatchResult.get_captured_tensor``.

### Function Prototype

```python
@dataclass(frozen=True)
class NodeIo:
    node: Node
    index: int = 0
```

### Attribute Description

| Attribute | Type | Description |
| :--- | :--- | :--- |
| `node` | `Node` | Node object, type is ``ge.graph.Node``. |
| `index` | `int` | Node output index, default is ``0``. |

---

## create_replacement Function

Create replacement graph, used to provide replacement subgraph in pattern fusion or operator decomposition.

### Function Prototype

```python
def create_replacement(graph: Graph) -> Graph:
    ...
```

### Parameter Description

| Parameter Name | Input/Output | Description |
| :---- | :-------- | :---- |
| graph | Input | Replacement graph, type is `ge.graph.Graph`. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `Graph` | Returns input replacement graph object. If input is not `ge.graph.Graph` type, will throw `TypeError`. |

---

## SubgraphInput Class

A logical boundary input, represents a set of main graph anchors (allows many-to-one). Used to describe input boundary of subgraph to be replaced.

### Function Prototype

```python
class SubgraphInput:
    def __init__(self) -> None: ...
    def __init__(self, node_inputs: typing.Iterable[tuple[Node, int]]) -> None: ...
    def add_input(self, node: Node, out_index: int) -> int: ...
```

### Method Description

| Method | Description |
| :--- | :--- |
| `__init__() -> None` | Constructs empty input, then append anchors via ``add_input``. |
| `__init__(node_inputs) -> None` | Constructs from ``(node, out_index)`` iterable object. Each element must be 2-tuple, otherwise C++ layer throws ``RuntimeError``. |
| `add_input(node: Node, out_index: int) -> int` | Appends an input anchor (node output index), returns internal index (``uint32`` semantics). |

---

## SubgraphOutput Class

A logical boundary output, represents single main graph node output anchor.

### Function Prototype

```python
class SubgraphOutput:
    def __init__(self) -> None: ...
    def __init__(self, node: Node, out_index: int) -> None: ...
    def set_output(self, node: Node, out_index: int) -> int: ...
```

### Method Description

| Method | Description |
| :--- | :--- |
| `__init__() -> None` | Constructs empty output, then call ``set_output`` to set. |
| `__init__(node, out_index) -> None` | Directly binds an output anchor ``(node, out_index)``. |
| `set_output(node: Node, out_index: int) -> int` | Sets output anchor, returns internal state (``uint32`` semantics). |

---

## SubgraphBoundary Class

Input/output boundary of subgraph to be replaced in main graph. Indices passed into ``add_input`` / ``add_output`` are **logical** boundary slots, must align with replacement graph input/output order.

### Function Prototype

```python
class SubgraphBoundary:
    def __init__(self) -> None: ...
    def add_input(self, index: int, input: SubgraphInput) -> int: ...
    def add_output(self, index: int, output: SubgraphOutput) -> int: ...
```

### Method Description

| Method | Description |
| :--- | :--- |
| `__init__() -> None` | Creates an empty boundary. |
| `add_input(index: int, input: SubgraphInput) -> int` | Binds the ``index``-th boundary input to ``SubgraphInput``. |
| `add_output(index: int, output: SubgraphOutput) -> int` | Binds the ``index``-th boundary output to ``SubgraphOutput``. |

---

## SubgraphRewriter Class

Execute whole subgraph replacement on main graph.

### SubgraphRewriter.replace Method (Static Method)

#### Function Prototype

```python
class SubgraphRewriter:
    @staticmethod
    def replace(boundary: SubgraphBoundary, replacement: Graph) -> int:
        ...
```

#### Parameter Description

| Parameter Name | Input/Output | Description |
| :---------- | :-------- | :---- |
| boundary | Input | Subgraph boundary, type is ``SubgraphBoundary``, describes input/output of subgraph to be replaced. |
| replacement | Input | Replacement graph, type is ``ge.graph.Graph``. |

#### Return Value Description

| Type | Description |
| :--- | :--- |
| `int` | Returns C++ ``Status`` integer form. When fails can check logs about boundary completeness and index alignment information. |

#### Constraint Description

- ``replacement`` must be non-empty graph wrapper; this graph will be **copied** in C++ side, but still need to follow GE's rules for Python ``Graph`` ownership.

#### Example

```python
from ge.passes import SubgraphBoundary, SubgraphInput, SubgraphOutput, SubgraphRewriter

# n0, out_node are nodes existing in main graph, replacement_graph is already built replacement graph
b = SubgraphBoundary()
b.add_input(0, SubgraphInput([(n0, 0), (n0, 1)]))
b.add_output(0, SubgraphOutput(out_node, 0))
ret = SubgraphRewriter.replace(b, replacement_graph)
```

For more usage, refer to https://gitcode.com/cann/ge/tree/master/examples/fusion_pass .---

## get_registered_passes Function

Get descriptor list of all registered Passes.

### Function Prototype

```python
def get_registered_passes() -> List[PassDescriptor]:
    ...
```

### Parameter Description

No parameters.

### Return Value Description

| Type | Description |
| :--- | :--- |
| `List[PassDescriptor]` | Returns list of registered `PassDescriptor` objects. |

---

## get_registered_pass_dicts Function

Get dictionary representation list of all registered Passes.

### Function Prototype

```python
def get_registered_pass_dicts() -> List[dict]:
    ...
```

### Parameter Description

No parameters.

### Return Value Description

| Type | Description |
| :--- | :--- |
| `List[dict]` | Returns dictionary list of registered Passes, each dictionary contains `descriptor_key`, `pass_name`, `module_name`, `class_name`, `stage`, `kind`, `op_types` and other fields. |

---

## get_registered_pass_by_descriptor_key Function

Get registered Pass descriptor by descriptor key.

### Function Prototype

```python
def get_registered_pass_by_descriptor_key(descriptor_key: str) -> Optional[PassDescriptor]:
    ...
```

### Parameter Description

| Parameter Name | Input/Output | Description |
| :------------- | :-------- | :---- |
| descriptor_key | Input | Pass descriptor key, string type, format is `{module_name}:{class_name}:{pass_name}`. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `Optional[PassDescriptor]` | Returns matching `PassDescriptor` object; if not found returns `None`. |

---

## clear_registered_passes Function

Clear all registered Passes.

### Function Prototype

```python
def clear_registered_passes() -> None:
    ...
```

### Parameter Description

No parameters.

### Return Value Description

No return value.

### Constraint Description

- This operation will clear entire Pass registry, all registered Passes will no longer be available after clearing.
