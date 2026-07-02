# Graph

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.graph import Graph
```

## 功能说明

Graph 类是 GE Python 接口的核心图操作类，用于管理计算图的构建、查询和修改。主要提供以下能力：

- **图生命周期管理**：创建、销毁计算图，支持图的序列化与反序列化。
- **节点查询**：获取图中所有节点或直接节点，按名称查找节点。
- **属性管理**：获取和设置图级别的自定义属性。
- **边操作**：添加/删除数据边和控制边，构建节点间的数据依赖和控制依赖关系。
- **子图管理**：添加、查询、删除子图，支持层级化的图结构。
- **序列化与持久化**：将图导出为文件（dump_to_file）或字符串（dump_to_stream），支持保存和加载 AIR 格式模型。

## 函数原型

### `__init__`

```python
def __init__(self, name: Optional[str] = "graph") -> None
```

创建一个 Graph 对象。

### `name`（属性）

```python
@property
def name(self) -> str
```

获取图名称。

### `get_all_nodes`

```python
def get_all_nodes(self) -> List[Node]
```

获取图中所有节点，包括子图中的节点。

### `get_direct_nodes`

```python
def get_direct_nodes(self) -> List[Node]
```

获取当前图的直接节点，不包含子图中的节点。

### `get_attr`

```python
def get_attr(self, key: str) -> Any
```

获取图的指定属性值。

### `set_attr`

```python
def set_attr(self, key: str, value: Any) -> None
```

设置图的指定属性。

### `dump_to_file`

```python
def dump_to_file(self, format: DumpFormat = DumpFormat.kReadable, suffix: str = "") -> None
```

将图导出到文件。

### `dump_to_stream`

```python
def dump_to_stream(self, format: DumpFormat = DumpFormat.kReadable) -> str
```

将图导出为字符串。

### `save_to_air`

```python
def save_to_air(self, file_path: str) -> None
```

将图保存为 AIR 格式文件。

### `load_from_air`

```python
def load_from_air(self, file_path: str) -> None
```

从 AIR 格式文件加载图。

### `remove_node`

```python
def remove_node(self, node: Node) -> None
```

从图中移除指定节点。

### `remove_edge`

```python
def remove_edge(self, src_node: Node, src_port_index: int, dst_node: Node, dst_port_index: int) -> None
```

移除指定的边。

### `add_data_edge`

```python
def add_data_edge(self, src_node: Node, src_port_index: int, dst_node: Node, dst_port_index: int) -> None
```

添加数据边。

### `add_control_edge`

```python
def add_control_edge(self, src_node: Node, dst_node: Node) -> None
```

添加控制边。

### `find_node_by_name`

```python
def find_node_by_name(self, name: str) -> Node
```

根据节点名称查找节点。

### `get_all_subgraphs`

```python
def get_all_subgraphs(self) -> List[Graph]
```

获取图中所有子图。

### `get_subgraph`

```python
def get_subgraph(self, name: str) -> Optional[Graph]
```

根据名称获取指定子图。

### `add_subgraph`

```python
def add_subgraph(self, subgraph: Graph) -> None
```

向图中添加子图。

### `remove_subgraph`

```python
def remove_subgraph(self, name: str) -> None
```

根据名称移除子图。

## 参数说明

### `__init__`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| name | Optional[str] | 否 | "graph" | 图名称，必须为字符串类型。 |

### `name`

无参数（只读属性）。

### `get_all_nodes`

无参数。

### `get_direct_nodes`

无参数。

### `get_attr`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| key | str | 是 | - | 属性名称，必须为字符串类型。 |

### `set_attr`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| key | str | 是 | - | 属性名称，必须为字符串类型。 |
| value | Any | 是 | - | 属性值，支持多种数据类型。 |

### `dump_to_file`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| format | DumpFormat | 否 | DumpFormat.kReadable | 导出文件的格式，取值范围为：DumpFormat.kOnnx、DumpFormat.kTxt、DumpFormat.kReadable。 |
| suffix | str | 否 | "" | 文件名后缀，追加在生成的文件名末尾。例如 suffix 为 "xxxx" 时，文件名格式为 `ge_<format>_00000_<graph_name>_0_xxxx.<ext>`。 |

DumpFormat 枚举值说明：

| 枚举值 | 数值 | 说明 |
| :----- | :--- | :--- |
| DumpFormat.kOnnx | 0 | ONNX 文本格式（pbtxt），仅包含图结构，不包含权重数据或其他属性。 |
| DumpFormat.kTxt | 1 | 文本格式。 |
| DumpFormat.kReadable | 2 | 可读格式（默认）。 |

### `dump_to_stream`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| format | DumpFormat | 否 | DumpFormat.kReadable | 导出字符串的格式，取值范围为：DumpFormat.kOnnx、DumpFormat.kTxt、DumpFormat.kReadable。 |

### `save_to_air`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| file_path | str | 是 | - | AIR 文件的保存路径，必须为字符串类型。 |

### `load_from_air`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| file_path | str | 是 | - | AIR 文件的加载路径，必须为字符串类型。 |

### `remove_node`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| node | Node | 是 | - | 待移除的节点对象，必须为 Node 类型。 |

### `remove_edge`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| src_node | Node | 是 | - | 边的源节点，必须为 Node 类型。 |
| src_port_index | int | 是 | - | 源节点的输出端口索引。移除控制边时，应设置为 -1。 |
| dst_node | Node | 是 | - | 边的目标节点，必须为 Node 类型。 |
| dst_port_index | int | 是 | - | 目标节点的输入端口索引。移除控制边时，应设置为 -1。 |

### `add_data_edge`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| src_node | Node | 是 | - | 数据边的源节点，必须为 Node 类型。 |
| src_port_index | int | 是 | - | 源节点的输出端口索引，必须为整数。 |
| dst_node | Node | 是 | - | 数据边的目标节点，必须为 Node 类型。 |
| dst_port_index | int | 是 | - | 目标节点的输入端口索引，必须为整数。 |

### `add_control_edge`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| src_node | Node | 是 | - | 控制边的源节点，必须为 Node 类型。 |
| dst_node | Node | 是 | - | 控制边的目标节点，必须为 Node 类型。 |

### `find_node_by_name`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| name | str | 是 | - | 节点名称，必须为字符串类型。 |

### `get_all_subgraphs`

无参数。

### `get_subgraph`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| name | str | 是 | - | 子图名称，必须为字符串类型。 |

### `add_subgraph`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| subgraph | Graph | 是 | - | 待添加的子图对象，必须为 Graph 类型。 |

### `remove_subgraph`

| 参数名 | 类型 | 是否必选 | 默认值 | 说明 |
| :----- | :--- | :------: | :----- | :--- |
| name | str | 是 | - | 待移除的子图名称，必须为字符串类型。 |

## 返回值说明

| 方法 | 返回类型 | 说明 |
| :--- | :------- | :--- |
| `__init__` | None | 无返回值。创建成功则返回 Graph 对象；创建失败则抛出异常。 |
| `name` | str | 返回图名称字符串。 |
| `get_all_nodes` | List[Node] | 返回图中所有节点列表（包含子图中的节点）。若图为空则返回空列表。 |
| `get_direct_nodes` | List[Node] | 返回当前图的直接节点列表（不包含子图中的节点）。若图为空则返回空列表。 |
| `get_attr` | Any | 返回指定属性名称对应的属性值。 |
| `set_attr` | None | 无返回值。设置失败则抛出异常。 |
| `dump_to_file` | None | 无返回值。导出失败则抛出异常。 |
| `dump_to_stream` | str | 返回图的字符串表示。 |
| `save_to_air` | None | 无返回值。保存失败则抛出异常。 |
| `load_from_air` | None | 无返回值。加载失败则抛出异常。 |
| `remove_node` | None | 无返回值。移除失败则抛出异常。 |
| `remove_edge` | None | 无返回值。移除失败则抛出异常。 |
| `add_data_edge` | None | 无返回值。添加失败则抛出异常。 |
| `add_control_edge` | None | 无返回值。添加失败则抛出异常。 |
| `find_node_by_name` | Node | 返回找到的节点对象。未找到则抛出异常。 |
| `get_all_subgraphs` | List[Graph] | 返回所有子图列表。若没有子图则返回空列表。 |
| `get_subgraph` | Optional[Graph] | 返回指定名称的子图对象。若未找到则返回 None。 |
| `add_subgraph` | None | 无返回值。添加失败则抛出异常。 |
| `remove_subgraph` | None | 无返回值。移除失败则抛出异常。 |

## 约束说明

- **所有权模型**：Graph 对象存在两种所有权状态。默认情况下由 Python 侧管理 C++ 资源的生命周期。当 Graph 作为子图参数传递给算子（如 If、While、Case）时，所有权会自动转移至 C++ 侧，以避免双重释放问题。
- **禁止拷贝**：Graph 类不支持拷贝操作（浅拷贝和深拷贝均不支持），调用 `copy` 或 `deepcopy` 将抛出 RuntimeError。
- **子图名称唯一性**：调用 `add_subgraph` 添加子图时，子图名称在父图中必须唯一。若名称已存在，操作将失败并抛出异常。
- **边的端口索引**：调用 `remove_edge` 移除控制边时，`src_port_index` 和 `dst_port_index` 均应设置为 -1。移除数据边时，端口索引必须与实际连接的端口一致。
- **dump_to_file 输出限制**：使用 DumpFormat.kOnnx 格式导出时，pbtxt 文件仅包含图结构信息，不包含权重数据或其他属性。
- **类型校验**：所有方法的参数均进行类型校验，类型不匹配时将抛出 TypeError；操作失败时将抛出 RuntimeError。
- **节点查找**：`find_node_by_name` 在未找到指定名称的节点时会抛出 RuntimeError，而非返回 None。使用前需确认节点确实存在。
