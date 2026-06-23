# Node

## Product Support Status

| Product | Support Status |
| :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.graph import Node
```

## Functionality Description

Node class represents a node in computation graph, provides node attribute read/write, input/output descriptor query and update, upstream/downstream node traversal and other capabilities.

Node objects cannot be directly instantiated (calling constructor will throw RuntimeError), need to be obtained through Graph class methods (such as get_all_nodes, find_node_by_name). Node objects do not support copy and deep copy.

## Function Prototypes

### Properties

```python
@property
def name(self) -> str
```

```python
@property
def type(self) -> str
```

### Methods

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
```def get_output_desc(self, index: int) -> TensorDesc
```

```python
def update_output_desc(self, index: int, tensor_desc: TensorDesc) -> None
```

## Parameter Description

### name

No parameters. This property is read-only, used to get node name.

### type

No parameters. This property is read-only, used to get node type.

### get_in_control_nodes

No parameters.

### get_out_control_nodes

No parameters.

### get_in_data_nodes_and_port_indexes

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| in_index | int | Required | Input port index number |

### get_out_data_nodes_and_port_indexes

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| out_index | int | Required | Output port index number |

### get_inputs_size

No parameters.

### get_outputs_size

No parameters.

### has_attr

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| attr_name | str | Required | Attribute name |

### get_attr

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| key | str | Required | Attribute name |

### set_attr

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| key | str | Required | Attribute name |
| value | Any | Required | Attribute value, supports string, number, list, Tensor etc. types |

### get_input_attr

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| attr_name | str | Required | Attribute name |
| input_index | int | Required | Input port index number |

### set_input_attr

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| attr_name | str | Required | Attribute name |
| input_index | int | Required | Input port index number |
| value | Any | Required | Attribute value |

### get_output_attr

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| attr_name | str | Required | Attribute name |
| output_index | int | Required | Output port index number |

### set_output_attr

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| attr_name | str | Required | Attribute name |
| output_index | int | Required | Output port index number |
| value | Any | Required | Attribute value |

### get_input_desc

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| index | int | Required | Input port index number |

### update_input_desc

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| index | int | Required | Input port index number |
| tensor_desc | TensorDesc | Required | New input tensor descriptor |

### get_output_desc

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| index | int | Required | Output port index number |

### update_output_desc

| Parameter | Type | Required/Optional | Description |
| :------ | :--- | :-------- | :--- |
| index | int | Required | Output port index number |
| tensor_desc | TensorDesc | Required | New output tensor descriptor |

## Return Value Description

| Method/Property | Return Type | Description |
| :------- | :------- | :--- |
| name | str | Node name |
| type | str | Node type |
| get_in_control_nodes | List[Node] | Input control edge node list, returns empty list if none |
| get_out_control_nodes | List[Node] | Output control edge node list, returns empty list if none |
| get_in_data_nodes_and_port_indexes | Tuple[Node, int] | Tuple composed of input data node and corresponding port number |
| get_out_data_nodes_and_port_indexes | List[Tuple[Node, int]] | Tuple list composed of output data node and corresponding port number |
| get_inputs_size | int | Node input count |
| get_outputs_size | int | Node output count |
| has_attr | bool | Whether node has specified name attribute, returns True if exists, False if not |
| get_attr | Any | Attribute value of specified name, supports string, number, list, Tensor etc. types |
| set_attr | None | No return value |
| get_input_attr | Any | Attribute value of specified input port |
| set_input_attr | None | No return value |
| get_output_attr | Any | Attribute value of specified output port |
| set_output_attr | None | No return value |
| get_input_desc | TensorDesc | Tensor descriptor of specified input port |
| update_input_desc | None | No return value |
| get_output_desc | TensorDesc | Tensor descriptor of specified output port |
| update_output_desc | None | No return value |

## Constraint Description

- Node object cannot be instantiated directly through constructor, calling `Node()` will throw RuntimeError, need to get Node object through Graph class methods (like get_all_nodes, find_node_by_name).
- Node object doesn't support copy and deepcopy, calling will throw RuntimeError.
- name and type are read-only properties, don't support modification.
- get_in_data_nodes_and_port_indexes's in_index parameter must be integer (int), otherwise throws TypeError; if get fails throws RuntimeError.
- get_out_data_nodes_and_port_indexes's out_index parameter must be integer (int), otherwise throws TypeError; if get fails throws RuntimeError.
- has_attr's attr_name parameter must be string (str), otherwise throws TypeError.
- get_attr's key parameter must be string (str), otherwise throws TypeError; if attribute get fails throws RuntimeError.
- set_attr's key parameter must be string (str), otherwise throws TypeError; if attribute set fails throws RuntimeError.
- get_input_attr's attr_name parameter must be string (str), input_index parameter must be integer (int), otherwise throws TypeError; if attribute get fails throws RuntimeError.
- set_input_attr's attr_name parameter must be string (str), input_index parameter must be integer (int), otherwise throws TypeError; if attribute set fails throws RuntimeError.
- get_output_attr's attr_name parameter must be string (str), output_index parameter must be integer (int), otherwise throws TypeError; if attribute get fails throws RuntimeError.
- set_output_attr's attr_name parameter must be string (str), output_index parameter must be integer (int), otherwise throws TypeError; if attribute set fails throws RuntimeError.
- get_input_desc's index parameter must be integer (int), otherwise throws TypeError; if get fails throws RuntimeError.
- update_input_desc's index parameter must be integer (int), tensor_desc parameter must be TensorDesc type, otherwise throws TypeError; if update fails throws RuntimeError.
- get_output_desc's index parameter must be integer (int), otherwise throws TypeError; if get fails throws RuntimeError.