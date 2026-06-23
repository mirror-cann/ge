# GeUtils

## Product Support Status

| Product | Support Status |
| :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.utils import GeUtils
```

## Functionality Description

`GeUtils` provides GE (Graph Engine) common utility interfaces, including Shape derivation and AICore operator support checking functionality.

- `infer_shape`: Performs Shape derivation on computation graph. This interface only executes Shape derivation, does not perform other graph optimizations (such as constant folding, dead edge elimination etc.).
- `check_node_support_on_aicore`: Checks if node is supported on AICore, returns boolean value for support status and unsupported reason string.

## Classes

### GeUtils

`GeUtils` class only contains static methods, can be used without instantiation.

| Static Method | Description |
| :--- | :--- |
| infer_shape | Performs Shape derivation on computation graph |
| check_node_support_on_aicore | Checks if node is supported on AICore |

## Function Prototypes

### infer_shape

```python
@staticmethod
def infer_shape(graph: Graph, input_shapes: List[List[int]]) -> None
```

Performs Shape derivation on computation graph. Only executes Shape derivation, does not perform constant folding, dead edge elimination and other graph optimizations.

### check_node_support_on_aicore

```python
@staticmethod
def check_node_support_on_aicore(node: Node) -> Tuple[bool, str]
```

Checks if node is supported on AICore, returns boolean value for support status and unsupported reason.

## Parameter Description

### infer_shape

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| graph | Graph | Yes | Computation graph object to perform Shape derivation |
| input_shapes | List[List[int]] | Yes | Input shape list. Each element in list describes shape of a graph input, each shape is integer dimension list |

### check_node_support_on_aicore

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| node | Node | Yes | Computation graph node object to be checked |

## Return Value Description

### infer_shape

No return value. Shape derivation result is directly updated in graph object.

### check_node_support_on_aicore

| Return Value | Type | Description |
| :--- | :--- | :--- |
| is_supported | bool | Whether node is supported on AICore. True means supported, False means not supported |
| unsupported_reason | str | Description of unsupported reason. If node is supported, returns empty string |

## Constraint Description

- `infer_shape` only executes Shape derivation, does not perform constant folding, dead edge elimination and other graph optimization operations.
- `input_shapes` for `infer_shape` must be list of lists, each sublist elements must be integer type.
- `check_node_support_on_aicore` requires passing valid `Node` object.
- All methods in `GeUtils` are static methods, can be called without creating instance.

## Usage Example

```python
from ge.utils import GeUtils
from ge.graph import Graph, Node

# Shape derivation
graph = Graph("my_graph")
# ... build graph ...
GeUtils.infer_shape(graph, [[1, 3, 224, 224], [1, 3, 224, 224]])

# Check if node is supported on AICore