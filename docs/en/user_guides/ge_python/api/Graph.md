# Graph

## Product Support Status

| Product | Support Status |
| :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.graph import Graph
```

## Functionality Description

Graph class is the core graph operation class in GE Python interface, used to manage computation graph construction, query and modification. Mainly provides the following capabilities:

- **Graph Lifecycle Management**: Create, destroy computation graphs, supports graph serialization and deserialization.
- **Node Query**: Get all nodes or direct nodes in graph, find nodes by name.
- **Attribute Management**: Get and set graph-level custom attributes.
- **Edge Operations**: Add/remove data edges and control edges, build data dependencies and control dependencies between nodes.
- **Subgraph Management**: Add, query, delete subgraphs, supports hierarchical graph structure.
- **Serialization and Persistence**: Export graph to file (dump_to_file) or string (dump_to_stream), supports saving and loading AIR format models.

## Function Prototypes

### `__init__`

```python
def __init__(self, name: Optional[str] = "graph") -> None
```

Creates a Graph object.

### `name` (property)

```python
@property
def name(self) -> str
```

Gets graph name.

### `get_all_nodes`

```python
def get_all_nodes(self) -> List[Node]
```

Gets all nodes in graph, including nodes in subgraphs.

### `get_direct_nodes`

```python
def get_direct_nodes(self) -> List[Node]
```

Gets direct nodes in current graph, not including nodes in subgraphs.

### `get_attr`

```python
def get_attr(self, key: str) -> Any
```

Gets specified attribute value of graph.

### `set_attr`

```python
def set_attr(self, key: str, value: Any) -> None
```

Sets specified attribute of graph.

### `dump_to_file`

```python
def dump_to_file(self, format: DumpFormat = DumpFormat.kReadable, suffix: str = "") -> None
```

Exports graph to file.

### `dump_to_stream`

```python
def dump_to_stream(self, format: DumpFormat = DumpFormat.kReadable) -> str
```

Exports graph to string.

### `save_to_air`

```python
def save_to_air(self, file_path: str) -> None
```

Saves graph as AIR format file.### `load_from_air`

```python
def load_from_air(self, file_path: str) -> None
```

Loads graph from AIR format file.

### `remove_node`

```python
def remove_node(self, node: Node) -> None
```

Removes specified node from graph.

### `remove_edge`

```python
def remove_edge(self, src_node: Node, src_port_index: int, dst_node: Node, dst_port_index: int) -> None
```

Removes specified edge.

### `add_data_edge`

```python
def add_data_edge(self, src_node: Node, src_port_index: int, dst_node: Node, dst_port_index: int) -> None
```

Adds data edge.

### `add_control_edge`

```python
def add_control_edge(self, src_node: Node, dst_node: Node) -> None
```

Adds control edge.

### `find_node_by_name`

```python
def find_node_by_name(self, name: str) -> Node
```

Finds node by node name.

### `get_all_subgraphs`

```python
def get_all_subgraphs(self) -> List[Graph]
```

Gets all subgraphs in graph.

### `get_subgraph`

```python
def get_subgraph(self, name: str) -> Optional[Graph]
```

Gets specified subgraph by name.

### `add_subgraph`

```python
def add_subgraph(self, subgraph: Graph) -> None
```

Adds subgraph to graph.

### `remove_subgraph`

```python
def remove_subgraph(self, name: str) -> None
```

Removes subgraph by name.

## Parameter Description

### `__init__`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| name | Optional[str] | No | "graph" | Graph name, must be string type. |

### `name`

No parameters (read-only property).

### `get_all_nodes`

No parameters.

### `get_direct_nodes`

No parameters.

### `get_attr`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| key | str | Yes | - | Attribute name, must be string type. |

### `set_attr`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| key | str | Yes | - | Attribute name, must be string type. |
| value | Any | Yes | - | Attribute value, supports multiple data types. |

### `dump_to_file`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| format | DumpFormat | No | DumpFormat.kReadable | Export file format, valid values: DumpFormat.kOnnx、DumpFormat.kTxt、DumpFormat.kReadable. |
| suffix | str | No | "" | Filename suffix, appended to end of generated filename. For example, when suffix is "xxxx", filename format is `ge_<format>_00000_<graph_name>_0_xxxx.<ext>`. |

DumpFormat enum value description:

| Enum Value | Numeric Value | Description |
| :----- | :--- | :--- |
| DumpFormat.kOnnx | 0 | ONNX text format (pbtxt), contains only graph structure, no weight data or other attributes. |
| DumpFormat.kTxt | 1 | Text format. |
| DumpFormat.kReadable | 2 | Readable format (default). |

### `dump_to_stream`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| format | DumpFormat | No | DumpFormat.kReadable | Export string format, valid values: DumpFormat.kOnnx、DumpFormat.kTxt、DumpFormat.kReadable. |

### `save_to_air`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| file_path | str | Yes | - | AIR file save path, must be string type. |

### `load_from_air`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| file_path | str | Yes | - | AIR file load path, must be string type. |

### `remove_node`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| node | Node | Yes | - | Node object to be removed, must be Node type. |

### `remove_edge`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| src_node | Node | Yes | - | Edge source node, must be Node type. |
| src_port_index | int | Yes | - | Source node output port index. When removing control edge, should be set to -1. |
| dst_node | Node | Yes | - | Edge target node, must be Node type. |
| dst_port_index | int | Yes | - | Target node input port index. When removing control edge, should be set to -1. |

### `add_data_edge`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| src_node | Node | Yes | - | Data edge source node, must be Node type. |
| src_port_index | int | Yes | - | Source node output port index, must be integer. |
| dst_node | Node | Yes | - | Data edge target node, must be Node type. |
| dst_port_index | int | Yes | - | Target node input port index, must be integer. |

### `add_control_edge`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| src_node | Node | Yes | - | Control edge source node, must be Node type. |
| dst_node | Node | Yes | - | Control edge target node, must be Node type. |

### `find_node_by_name`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| name | str | Yes | - | Node name, must be string type. |

### `get_all_subgraphs`

No parameters.

### `get_subgraph`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| name | str | Yes | - | Subgraph name, must be string type. |

### `add_subgraph`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| subgraph | Graph | Yes | - | Subgraph object to be added, must be Graph type. |### `remove_subgraph`

| Parameter Name | Type | Required | Default Value | Description |
| :----- | :--- | :------: | :----- | :--- |
| name | str | Yes | - | Subgraph name to be removed, must be string type. |

## Return Value Description

| Method | Return Type | Description |
| :--- | :------- | :--- |
| `__init__` | None | No return value. Returns Graph object on successful creation; throws exception on failure. |
| `name` | str | Returns graph name string. |
| `get_all_nodes` | List[Node] | Returns list of all nodes in graph (including nodes in subgraphs). Returns empty list if graph is empty. |
| `get_direct_nodes` | List[Node] | Returns list of direct nodes in current graph (excluding nodes in subgraphs). Returns empty list if graph is empty. |
| `get_attr` | Any | Returns attribute value corresponding to specified attribute name. |
| `set_attr` | None | No return value. Throws exception on failure. |
| `dump_to_file` | None | No return value. Throws exception on export failure. |
| `dump_to_stream` | str | Returns string representation of graph. |
| `save_to_air` | None | No return value. Throws exception on save failure. |
| `load_from_air` | None | No return value. Throws exception on load failure. |
| `remove_node` | None | No return value. Throws exception on removal failure. |
| `remove_edge` | None | No return value. Throws exception on removal failure. |
| `add_data_edge` | None | No return value. Throws exception on addition failure. |
| `add_control_edge` | None | No return value. Throws exception on addition failure. |
| `find_node_by_name` | Node | Returns found node object. Throws exception if not found. |
| `get_all_subgraphs` | List[Graph] | Returns list of all subgraphs. Returns empty list if no subgraphs exist. |
| `get_subgraph` | Optional[Graph] | Returns subgraph object with specified name. Returns None if not found. |
| `add_subgraph` | None | No return value. Throws exception on addition failure. |
| `remove_subgraph` | None | No return value. Throws exception on removal failure. |

## Constraint Description

- **Ownership Model**: Graph objects have two ownership states. By default, Python side manages C++ resource lifecycle. When Graph is passed as subgraph parameter to operator (like If、While、Case), ownership automatically transfers to C++ side to avoid double free issues.
- **No Copy**: Graph class does not support copy operations (both shallow copy and deep copy are not supported), calling `copy` or `deepcopy` will throw RuntimeError.
- **Subgraph Name Uniqueness**: When calling `add_subgraph` to add subgraph, subgraph name must be unique in parent graph. If name already exists, operation will fail and throw exception.
- **Edge Port Index**: When calling `remove_edge` to remove control edge, both `src_port_index` and `dst_port_index` should be set to -1. When removing data edge, port index must match the actual connected port.
- **dump_to_file Output Limitation**: When exporting with DumpFormat.kOnnx format, pbtxt file only contains graph structure information, no weight data or other attributes.
- **Type Validation**: All method parameters undergo type validation, throws TypeError on type mismatch; throws RuntimeError on operation failure.
- **Node Lookup**: `find_node_by_name` throws RuntimeError when specified name node is not found, instead of returning None. Need to confirm node exists before use.
