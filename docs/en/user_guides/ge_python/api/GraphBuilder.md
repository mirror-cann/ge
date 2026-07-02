# GraphBuilder

## Product Support Status

| Product | Support Status |
| | :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.es import GraphBuilder
```

## Functionality Description

`GraphBuilder` is an Eager-Style (immediate style) graph builder, providing functional-style computation graph construction methods. Through `create_*` series methods create various tensors (input, constant, scalar, vector etc.), through `set_graph_output` set graph output, finally through `build_and_reset` build and return `Graph` object.

After calling `build_and_reset`, `GraphBuilder` enters built state, cannot create new tensors. To build new computation graph, need to create new `GraphBuilder` instance.

## Enumerations

### InputType

Input type enumeration, used to specify graph input node type.

| Enumeration Value | Description |
| | :--- | :--- |
| DATA | Normal data input |
| REF_DATA | Reference data input |
| AIPP_DATA | AIPP (AI Pre-Processing) data input |
| ANY_DATA | Any type data input |

## Classes

### GraphBuilder

GraphBuilder class used to build computation graphs, does not support copy and deep copy.

| Method/Attribute | Description |
| | :--- | :--- |
| name (property) | Gets graph builder name, returns str |
| create_input | Creates graph input tensor |
| create_inputs | Batch creates graph input tensors |
| create_const_int64 | Creates int64 constant tensor |
| create_const_float | Creates float constant tensor |
| create_const_uint64 | Creates uint64 constant tensor |
| create_const_int32 | Creates int32 constant tensor |
| create_const_uint32 | Creates uint32 constant tensor |
| create_vector_int64 | Creates int64 vector tensor |
| create_scalar_int64 | Creates int64 scalar tensor |
| create_scalar_int32 | Creates int32 scalar tensor |
| create_scalar_float | Creates float scalar tensor |
| create_scalar_uint64 | Creates uint64 scalar tensor |
| create_scalar_uint32 | Creates uint32 scalar tensor |
| create_variable | Creates variable tensor |
| set_graph_output | Sets graph output |
| set_graph_attr_int64 | Sets graph-level int64 attribute |
| set_graph_attr_string | Sets graph-level string attribute |
| set_graph_attr_bool | Sets graph-level bool attribute |
| set_tensor_attr_int64 | Sets tensor-level int64 attribute |
| set_tensor_attr_string | Sets tensor-level string attribute |
| set_tensor_attr_bool | Sets tensor-level bool attribute |
| set_node_attr_int64 | Sets node-level int64 attribute |
| set_node_attr_string | Sets node-level string attribute |
| set_node_attr_bool | Sets node-level bool attribute |
| add_control_dependency | Adds control dependency |
| build_and_reset | Builds graph and resets builder state |

## Function Prototypes

### \_\_init\_\_

```python
def __init__(self, name: Optional[str] = None) -> None
```

### name

```python
@property
def name(self) -> str
```

### create_input

```python
def create_input(self, index: int, *, name: Optional[str] = None,
                 type_str: Optional[InputType] = InputType.DATA,
                 data_type: Optional[DataType] = DataType.DT_FLOAT,
                 format: Optional[Format] = Format.FORMAT_ND,
                 shape: Optional[List[int]] = None) -> TensorHolder
```

### create_inputs

```python
def create_inputs(self, num: int, start_index: int = 0) -> List[TensorHolder]
```

### create_const_int64

```python
def create_const_int64(self, value: Union[int, List[int]], shape: Optional[List[int]] = None) -> TensorHolder
```

### create_const_float

```python
def create_const_float(self, value: Union[float, List[float]], shape: Optional[List[int]] = None) -> TensorHolder
```

### create_const_uint64

```python
def create_const_uint64(self, value: Union[int, List[int]], shape: Optional[List[int]] = None) -> TensorHolder
```

### create_const_int32

```python
def create_const_int32(self, value: Union[int, List[int]], shape: Optional[List[int]] = None) -> TensorHolder
```

### create_const_uint32

```python
def create_const_uint32(self, value: Union[int, List[int]], shape: Optional[List[int]] = None) -> TensorHolder
```

### create_vector_int64

```python
def create_vector_int64(self, value: List[int]) -> TensorHolder
```

### create_scalar_int64

```python
def create_scalar_int64(self, value: int) -> TensorHolder
```

### create_scalar_int32

```python
def create_scalar_int32(self, value: int) -> TensorHolder
```

### create_scalar_float

```python
def create_scalar_float(self, value: float) -> TensorHolder
```

### create_scalar_uint64

```python
def create_scalar_uint64(self, value: int) -> TensorHolder
```

### create_scalar_uint32

```python
def create_scalar_uint32(self, value: int) -> TensorHolder
```

### create_variable

```python
def create_variable(self, index: int, name: str) -> TensorHolder
```

### set_graph_output

```python
def set_graph_output(self, tensor: TensorHolder, output_index: int) -> None
```

### set_graph_attr_int64

```python
def set_graph_attr_int64(self, attr_name: str, value: int) -> None
```

### set_graph_attr_string

```python
def set_graph_attr_string(self, attr_name: str, value: str) -> None
```

### set_graph_attr_bool

```python
def set_graph_attr_bool(self, attr_name: str, value: bool) -> None
```

### set_tensor_attr_int64

```python
def set_tensor_attr_int64(self, tensor: TensorHolder, attr_name: str, value: int) -> None
```

### set_tensor_attr_string

```python
def set_tensor_attr_string(self, tensor: TensorHolder, attr_name: str, value: str) -> None
```

### set_tensor_attr_bool

```python
def set_tensor_attr_bool(self, tensor: TensorHolder, attr_name: str, value: bool) -> None
```

### set_node_attr_int64

```python
def set_node_attr_int64(self, tensor: TensorHolder, attr_name: str, value: int) -> None
```

### set_node_attr_string

```python
def set_node_attr_string(self, tensor: TensorHolder, attr_name: str, value: str) -> None
```

### set_node_attr_bool

```python
def set_node_attr_bool(self, tensor: TensorHolder, attr_name: str, value: bool) -> None
```

### add_control_dependency

```python
def add_control_dependency(self, dst_tensor: TensorHolder, src_tensors: List[TensorHolder]) -> None
```

### build_and_reset

```python
def build_and_reset(self, outputs: Optional[List[TensorHolder]] = None) -> Graph
```

## Parameter Description

### \_\_init\_\_

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| name | Optional[str] | No | Graph name. Default is None, when name is "graph" |

### create_input

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| index | int | Yes | Input index, representing the sequence number of this input in the graph |
| name | Optional[str] | No | Input name. Default is None, when name is "input_{index}" |
| type_str | Optional[InputType] | No | Input type, default is InputType.DATA |
| data_type | Optional[DataType] | No | Data type, default is DataType.DT_FLOAT |
| format | Optional[Format] | No | Data format, default is Format.FORMAT_ND |
| shape | Optional[List[int]] | No | Shape dimension list. Default is None, representing scalar |

### create_inputs

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| num | int | Yes | Number of inputs to create, must be positive integer |
| start_index | int | No | Starting index, default is 0. Graph input node indices should start from 0 and increment continuously |

### create_const_int64 / create_const_uint64 / create_const_int32 / create_const_uint32

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| value | Union[int, List[int]] | Yes | Single integer or integer list. If list, element count must match shape dimension product |
| shape | Optional[List[int]] | No | Shape dimensions. If None: single integer creates scalar (shape=[]), list creates 1-D tensor (shape=[len(value)]). If specified, dimension product must equal list element count |

### create_const_float

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| value | Union[float, List[float]] | Yes | Single floating-point number or floating-point number list. If list, element count must match shape dimension product |
| shape | Optional[List[int]] | No | Shape dimensions. Rules same as other create_const_* methods |

### create_vector_int64

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| value | List[int] | Yes | Integer list, generates 1-D int64 tensor with shape [len(value)] |

### create_scalar_int64 / create_scalar_int32 / create_scalar_float / create_scalar_uint64 / create_scalar_uint32

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| value | int or float | Yes | Scalar value. uint64 requires non-negative integer, uint32 requires value in [0, 2^32-1] range |

### create_variable

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| index | int | Yes | Variable index |
| name | str | Yes | Variable name |

### set_graph_output

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| tensor | TensorHolder | Yes | Tensor object to set as output |
| output_index | int | Yes | Output index |

### set_graph_attr_int64 / set_tensor_attr_int64 / set_node_attr_int64

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| attr_name (or tensor + attr_name) | str (TensorHolder + str) | Yes | Attribute name (when setting tensor/node attribute, also need to pass corresponding TensorHolder) |
| value | int | Yes | int64 attribute value |

### set_graph_attr_string / set_tensor_attr_string / set_node_attr_string

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| attr_name (or tensor + attr_name) | str (TensorHolder + str) | Yes | Attribute name (when setting tensor/node attribute, also need to pass corresponding TensorHolder) |
| value | str | Yes | String attribute value |

### set_graph_attr_bool / set_tensor_attr_bool / set_node_attr_bool

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| attr_name (or tensor + attr_name) | str (TensorHolder + str) | Yes | Attribute name (when setting tensor/node attribute, also need to pass corresponding TensorHolder) |
| value | bool | Yes | Boolean attribute value |

### add_control_dependency

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| dst_tensor | TensorHolder | Yes | Target tensor, will add control dependency node |
| src_tensors | List[TensorHolder] | Yes | Source tensor list, control dependency source nodes |

### build_and_reset

| Parameter | Type | Required | Description |
| | :--- | :--- | :---: | :--- |
| outputs | Optional[List[TensorHolder]] | No | Output tensor list. If passed, automatically sets output in order before building (indices start from 0). Default is None, uses previously set output |

## Return Value Description

| Method | Return Type | Description |
| | :--- | :--- | :--- |
| name (property) | str | Graph builder name |
| create_input | TensorHolder | Tensor object representing input |
| create_inputs | List[TensorHolder] | Input tensor object list, all elements are DataType.DT_FLOAT, Format.FORMAT_ND, shape=[] |
| create_const_* | TensorHolder | Tensor object representing constant |
| create_vector_int64 | TensorHolder | Tensor object representing int64 vector |
| create_scalar_* | TensorHolder | Tensor object representing scalar |
| create_variable | TensorHolder | Tensor object representing variable |
| set_graph_output | None | No return value |
| set_graph_attr_* | None | No return value |
| set_tensor_attr_* | None | No return value |
| set_node_attr_* | None | No return value |
| add_control_dependency | None | No return value |
| build_and_reset | Graph | Completed computation graph object |

## Constraint Description

- After calling `build_and_reset`, `GraphBuilder` enters built state, cannot create new tensors or set attributes. To build new computation graph, please create new `GraphBuilder` instance.
- `GraphBuilder` does not support copy and deep copy.
- All `TensorHolder` objects created by `GraphBuilder` hold reference to builder, builder will not be garbage collected as long as any tensor is still referenced.
- Inputs created by `create_inputs` default data type is `DataType.DT_FLOAT`, format is `Format.FORMAT_ND`, shape is scalar (`[]`).
- Graph input node indices should start from 0 and increment continuously.
- In `create_const_*` methods, if both `value` is list and `shape` is specified, list element count must equal `shape` dimension product.
- `create_scalar_uint64` value must be non-negative integer; `create_scalar_uint32` value must be in [0, 4294967295] range.

## Usage Example

```python
from ge.es import GraphBuilder, InputType
from ge.graph.types import DataType, Format

# Create graph builder
builder = GraphBuilder("my_graph")

# Create inputs
input_tensor = builder.create_input(0, name="x", data_type=DataType.DT_FLOAT, format=Format.FORMAT_ND)
inputs = builder.create_inputs(2, start_index=1)

# Create constants
const_float = builder.create_const_float(1.0)
const_int_list = builder.create_const_int64([1, 2, 3], shape=[1, 3])
scalar = builder.create_scalar_int64(42)

# Create vector
vec = builder.create_vector_int64([10, 20, 30])

# Create variable
var = builder.create_variable(0, "my_var")

# Set graph attribute
builder.set_graph_attr_string("attr_key", "attr_value")

# Set tensor attribute
builder.set_tensor_attr_int64(input_tensor, "tensor_attr", 100)

# Set node attribute
builder.set_node_attr_bool(input_tensor, "node_attr", True)

# Add control dependency
builder.add_control_dependency(input_tensor, [const_float])

# Set graph output
builder.set_graph_output(input_tensor, 0)

# Build graph
graph = builder.build_and_reset()

# Or specify output directly when building
# graph = builder.build_and_reset(outputs=[input_tensor])
```
