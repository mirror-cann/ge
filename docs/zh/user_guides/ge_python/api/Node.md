# Node

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.graph import Node
```

## 功能说明

Node 类表示计算图中的节点，提供节点属性读写、输入输出描述符查询和更新、上下游节点遍历等能力。

Node 对象不能直接实例化（调用构造函数会抛出 RuntimeError），需要通过 Graph 类的方法（如 get_all_nodes、find_node_by_name）获取。Node 对象不支持拷贝（copy）和深拷贝（deepcopy）。

## 函数原型

### 属性

```python
@property
def name(self) -> str
```

```python
@property
def type(self) -> str
```

### 方法

```python
def get_in_control_nodes(self) -> List[Node]
```

```python
def get_out_control_nodes(self) -> List[Node]
```

```python
def get_in_data_nodes_and_port_indexes(self, in_index: int) -> Tuple[Node, int]
```

```python
def get_out_data_nodes_and_port_indexes(self, out_index: int) -> List[Tuple[Node, int]]
```

```python
def get_inputs_size(self) -> int
```

```python
def get_outputs_size(self) -> int
```

```python
def has_attr(self, attr_name: str) -> bool
```

```python
def get_attr(self, key: str) -> Any
```

```python
def set_attr(self, key: str, value: Any) -> None
```

```python
def get_input_attr(self, attr_name: str, input_index: int) -> Any
```

```python
def set_input_attr(self, attr_name: str, input_index: int, value: Any) -> None
```

```python
def get_output_attr(self, attr_name: str, output_index: int) -> Any
```

```python
def set_output_attr(self, attr_name: str, output_index: int, value: Any) -> None
```

```python
def get_input_desc(self, index: int) -> TensorDesc
```

```python
def update_input_desc(self, index: int, tensor_desc: TensorDesc) -> None
```

```python
def get_output_desc(self, index: int) -> TensorDesc
```

```python
def update_output_desc(self, index: int, tensor_desc: TensorDesc) -> None
```

## 参数说明

### name

无参数。该属性为只读属性，用于获取节点名称。

### type

无参数。该属性为只读属性，用于获取节点类型。

### get_in_control_nodes

无参数。

### get_out_control_nodes

无参数。

### get_in_data_nodes_and_port_indexes

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| in_index | int | 必选 | 输入端口的索引号 |

### get_out_data_nodes_and_port_indexes

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| out_index | int | 必选 | 输出端口的索引号 |

### get_inputs_size

无参数。

### get_outputs_size

无参数。

### has_attr

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| attr_name | str | 必选 | 属性名称 |

### get_attr

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| key | str | 必选 | 属性名称 |

### set_attr

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| key | str | 必选 | 属性名称 |
| value | Any | 必选 | 属性值，支持 string、number、list、Tensor 等类型 |

### get_input_attr

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| attr_name | str | 必选 | 属性名称 |
| input_index | int | 必选 | 输入端口的索引号 |

### set_input_attr

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| attr_name | str | 必选 | 属性名称 |
| input_index | int | 必选 | 输入端口的索引号 |
| value | Any | 必选 | 属性值 |

### get_output_attr

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| attr_name | str | 必选 | 属性名称 |
| output_index | int | 必选 | 输出端口的索引号 |

### set_output_attr

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| attr_name | str | 必选 | 属性名称 |
| output_index | int | 必选 | 输出端口的索引号 |
| value | Any | 必选 | 属性值 |

### get_input_desc

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| index | int | 必选 | 输入端口的索引号 |

### update_input_desc

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| index | int | 必选 | 输入端口的索引号 |
| tensor_desc | TensorDesc | 必选 | 新的输入张量描述符 |

### get_output_desc

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| index | int | 必选 | 输出端口的索引号 |

### update_output_desc

| 参数 | 类型 | 必选/可选 | 说明 |
| :------ | :--- | :-------- | :--- |
| index | int | 必选 | 输出端口的索引号 |
| tensor_desc | TensorDesc | 必选 | 新的输出张量描述符 |

## 返回值说明

| 方法/属性 | 返回类型 | 说明 |
| :------- | :------- | :--- |
| name | str | 节点名称 |
| type | str | 节点类型 |
| get_in_control_nodes | List[Node] | 输入控制边节点列表，若无则返回空列表 |
| get_out_control_nodes | List[Node] | 输出控制边节点列表，若无则返回空列表 |
| get_in_data_nodes_and_port_indexes | Tuple[Node, int] | 由输入数据节点和对应端口号组成的元组 |
| get_out_data_nodes_and_port_indexes | List[Tuple[Node, int]] | 由输出数据节点和对应端口号组成的元组列表 |
| get_inputs_size | int | 节点的输入数量 |
| get_outputs_size | int | 节点的输出数量 |
| has_attr | bool | 节点是否具有指定名称的属性，存在返回 True，不存在返回 False |
| get_attr | Any | 指定名称的属性值，支持 string、number、list、Tensor 等类型 |
| set_attr | None | 无返回值 |
| get_input_attr | Any | 指定输入端口的属性值 |
| set_input_attr | None | 无返回值 |
| get_output_attr | Any | 指定输出端口的属性值 |
| set_output_attr | None | 无返回值 |
| get_input_desc | TensorDesc | 指定输入端口的张量描述符 |
| update_input_desc | None | 无返回值 |
| get_output_desc | TensorDesc | 指定输出端口的张量描述符 |
| update_output_desc | None | 无返回值 |

## 约束说明

- Node 对象不能通过构造函数直接实例化，调用 `Node()` 会抛出 RuntimeError，需要通过 Graph 类的方法（如 get_all_nodes、find_node_by_name）获取 Node 对象。
- Node 对象不支持拷贝（copy）和深拷贝（deepcopy），调用时会抛出 RuntimeError。
- name 和 type 为只读属性，不支持修改。
- get_in_data_nodes_and_port_indexes 的 in_index 参数必须为整数（int），否则抛出 TypeError；若获取失败则抛出 RuntimeError。
- get_out_data_nodes_and_port_indexes 的 out_index 参数必须为整数（int），否则抛出 TypeError；若获取失败则抛出 RuntimeError。
- has_attr 的 attr_name 参数必须为字符串（str），否则抛出 TypeError。
- get_attr 的 key 参数必须为字符串（str），否则抛出 TypeError；若属性获取失败则抛出 RuntimeError。
- set_attr 的 key 参数必须为字符串（str），否则抛出 TypeError；若属性设置失败则抛出 RuntimeError。
- get_input_attr 的 attr_name 参数必须为字符串（str）、input_index 参数必须为整数（int），否则抛出 TypeError；若属性获取失败则抛出 RuntimeError。
- set_input_attr 的 attr_name 参数必须为字符串（str）、input_index 参数必须为整数（int），否则抛出 TypeError；若属性设置失败则抛出 RuntimeError。
- get_output_attr 的 attr_name 参数必须为字符串（str）、output_index 参数必须为整数（int），否则抛出 TypeError；若属性获取失败则抛出 RuntimeError。
- set_output_attr 的 attr_name 参数必须为字符串（str）、output_index 参数必须为整数（int），否则抛出 TypeError；若属性设置失败则抛出 RuntimeError。
- get_input_desc 的 index 参数必须为整数（int），否则抛出 TypeError；若获取失败则抛出 RuntimeError。
- update_input_desc 的 index 参数必须为整数（int）、tensor_desc 参数必须为 TensorDesc 类型，否则抛出 TypeError；若更新失败则抛出 RuntimeError。
- get_output_desc 的 index 参数必须为整数（int），否则抛出 TypeError；若获取失败则抛出 RuntimeError。
- update_output_desc 的 index 参数必须为整数（int）、tensor_desc 参数必须为 TensorDesc 类型，否则抛出 TypeError；若更新失败则抛出 RuntimeError。
