# GraphBuilder

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.es import GraphBuilder
```

## 功能说明

`GraphBuilder` 是 Eager-Style（即时风格）图构建器，提供函数式风格的计算图构建方式。通过 `create_*` 系列方法创建各类张量（输入、常量、标量、向量等），通过 `set_graph_output` 设置图输出，最后通过 `build_and_reset` 构建并返回 `Graph` 对象。

调用 `build_and_reset` 后，`GraphBuilder` 进入已构建状态，不可再创建新的张量。如需构建新的计算图，需要创建新的 `GraphBuilder` 实例。

## 枚举

### InputType

输入类型枚举，用于指定图输入节点的类型。

| 枚举值 | 说明 |
| :--- | :--- |
| DATA | 普通数据输入 |
| REF_DATA | 引用数据输入 |
| AIPP_DATA | AIPP（AI Pre-Processing）数据输入 |
| ANY_DATA | 任意类型数据输入 |

## 类

### GraphBuilder

GraphBuilder 类用于构建计算图，不支持拷贝和深拷贝。

| 方法/属性 | 说明 |
| :--- | :--- |
| name（property） | 获取图构建器名称，返回 str |
| create_input | 创建图输入张量 |
| create_inputs | 批量创建图输入张量 |
| create_const_int64 | 创建 int64 常量张量 |
| create_const_float | 创建 float 常量张量 |
| create_const_uint64 | 创建 uint64 常量张量 |
| create_const_int32 | 创建 int32 常量张量 |
| create_const_uint32 | 创建 uint32 常量张量 |
| create_vector_int64 | 创建 int64 向量张量 |
| create_scalar_int64 | 创建 int64 标量张量 |
| create_scalar_int32 | 创建 int32 标量张量 |
| create_scalar_float | 创建 float 标量张量 |
| create_scalar_uint64 | 创建 uint64 标量张量 |
| create_scalar_uint32 | 创建 uint32 标量张量 |
| create_variable | 创建变量张量 |
| set_graph_output | 设置图输出 |
| set_graph_attr_int64 | 设置图级别 int64 属性 |
| set_graph_attr_string | 设置图级别 string 属性 |
| set_graph_attr_bool | 设置图级别 bool 属性 |
| set_tensor_attr_int64 | 设置张量级别 int64 属性 |
| set_tensor_attr_string | 设置张量级别 string 属性 |
| set_tensor_attr_bool | 设置张量级别 bool 属性 |
| set_node_attr_int64 | 设置节点级别 int64 属性 |
| set_node_attr_string | 设置节点级别 string 属性 |
| set_node_attr_bool | 设置节点级别 bool 属性 |
| add_control_dependency | 添加控制依赖 |
| build_and_reset | 构建图并重置构建器状态 |

## 函数原型

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

## 参数说明

### \_\_init\_\_

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| name | Optional[str] | 否 | 图名称。默认为 None，此时名称为 "graph" |

### create_input

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| index | int | 是 | 输入索引，表示该输入在图中的序号 |
| name | Optional[str] | 否 | 输入名称。默认为 None，此时名称为 "input_{index}" |
| type_str | Optional[InputType] | 否 | 输入类型，默认为 InputType.DATA |
| data_type | Optional[DataType] | 否 | 数据类型，默认为 DataType.DT_FLOAT |
| format | Optional[Format] | 否 | 数据格式，默认为 Format.FORMAT_ND |
| shape | Optional[List[int]] | 否 | 形状维度列表。默认为 None，表示标量 |

### create_inputs

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| num | int | 是 | 需要创建的输入数量，必须为正整数 |
| start_index | int | 否 | 起始索引，默认为 0。图的输入节点索引应从 0 开始连续递增 |

### create_const_int64 / create_const_uint64 / create_const_int32 / create_const_uint32

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| value | Union[int, List[int]] | 是 | 单个整数或整数列表。若为列表，元素数量须与 shape 维度乘积匹配 |
| shape | Optional[List[int]] | 否 | 形状维度。若为 None：单个整数创建标量（shape=[]），列表创建一维张量（shape=[len(value)]）。若指定，维度乘积须等于列表元素数量 |

### create_const_float

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| value | Union[float, List[float]] | 是 | 单个浮点数或浮点数列表。若为列表，元素数量须与 shape 维度乘积匹配 |
| shape | Optional[List[int]] | 否 | 形状维度。规则同其他 create_const_* 方法 |

### create_vector_int64

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| value | List[int] | 是 | 整数列表，生成 shape 为 [len(value)] 的一维 int64 张量 |

### create_scalar_int64 / create_scalar_int32 / create_scalar_float / create_scalar_uint64 / create_scalar_uint32

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| value | int 或 float | 是 | 标量值。uint64 要求非负整数，uint32 要求值在 [0, 2^32-1] 范围内 |

### create_variable

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| index | int | 是 | 变量索引 |
| name | str | 是 | 变量名称 |

### set_graph_output

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| tensor | TensorHolder | 是 | 要设置为输出的张量对象 |
| output_index | int | 是 | 输出索引 |

### set_graph_attr_int64 / set_tensor_attr_int64 / set_node_attr_int64

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| attr_name（或 tensor + attr_name） | str（TensorHolder + str） | 是 | 属性名称（设置张量/节点属性时还需传入对应的 TensorHolder） |
| value | int | 是 | int64 属性值 |

### set_graph_attr_string / set_tensor_attr_string / set_node_attr_string

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| attr_name（或 tensor + attr_name） | str（TensorHolder + str） | 是 | 属性名称（设置张量/节点属性时还需传入对应的 TensorHolder） |
| value | str | 是 | 字符串属性值 |

### set_graph_attr_bool / set_tensor_attr_bool / set_node_attr_bool

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| attr_name（或 tensor + attr_name） | str（TensorHolder + str） | 是 | 属性名称（设置张量/节点属性时还需传入对应的 TensorHolder） |
| value | bool | 是 | 布尔属性值 |

### add_control_dependency

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| dst_tensor | TensorHolder | 是 | 目标张量，将添加控制依赖的节点 |
| src_tensors | List[TensorHolder] | 是 | 源张量列表，控制依赖的来源节点 |

### build_and_reset

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| outputs | Optional[List[TensorHolder]] | 否 | 输出张量列表。若传入，则在构建前自动按顺序设置输出（索引从 0 开始）。默认为 None，使用之前已设置的输出 |

## 返回值说明

| 方法 | 返回值类型 | 说明 |
| :--- | :--- | :--- |
| name（property） | str | 图构建器名称 |
| create_input | TensorHolder | 表示输入的张量对象 |
| create_inputs | List[TensorHolder] | 输入张量对象列表，元素均为 DataType.DT_FLOAT、Format.FORMAT_ND、shape=[] |
| create_const_* | TensorHolder | 表示常量的张量对象 |
| create_vector_int64 | TensorHolder | 表示 int64 向量的张量对象 |
| create_scalar_* | TensorHolder | 表示标量的张量对象 |
| create_variable | TensorHolder | 表示变量的张量对象 |
| set_graph_output | None | 无返回值 |
| set_graph_attr_* | None | 无返回值 |
| set_tensor_attr_* | None | 无返回值 |
| set_node_attr_* | None | 无返回值 |
| add_control_dependency | None | 无返回值 |
| build_and_reset | Graph | 构建完成的计算图对象 |

## 约束说明

- 调用 `build_and_reset` 后，`GraphBuilder` 进入已构建状态，不可再创建新张量或设置属性。如需构建新的计算图，请创建新的 `GraphBuilder` 实例。
- `GraphBuilder` 不支持拷贝（copy）和深拷贝（deepcopy）。
- 所有由 `GraphBuilder` 创建的 `TensorHolder` 对象持有对构建器的引用，只要任一张量仍被引用，构建器就不会被垃圾回收。
- `create_inputs` 创建的输入默认数据类型为 `DataType.DT_FLOAT`、格式为 `Format.FORMAT_ND`、shape 为标量（`[]`）。
- 图的输入节点索引应从 0 开始连续递增。
- `create_const_*` 方法中，若同时指定 `value` 为列表和 `shape`，则列表元素数量必须等于 `shape` 各维度的乘积。
- `create_scalar_uint64` 的值必须为非负整数；`create_scalar_uint32` 的值必须在 [0, 4294967295] 范围内。

## 使用示例

```python
from ge.es import GraphBuilder, InputType
from ge.graph.types import DataType, Format

# 创建图构建器
builder = GraphBuilder("my_graph")

# 创建输入
input_tensor = builder.create_input(0, name="x", data_type=DataType.DT_FLOAT, format=Format.FORMAT_ND)
inputs = builder.create_inputs(2, start_index=1)

# 创建常量
const_float = builder.create_const_float(1.0)
const_int_list = builder.create_const_int64([1, 2, 3], shape=[1, 3])
scalar = builder.create_scalar_int64(42)

# 创建向量
vec = builder.create_vector_int64([10, 20, 30])

# 创建变量
var = builder.create_variable(0, "my_var")

# 设置图属性
builder.set_graph_attr_string("attr_key", "attr_value")

# 设置张量属性
builder.set_tensor_attr_int64(input_tensor, "tensor_attr", 100)

# 设置节点属性
builder.set_node_attr_bool(input_tensor, "node_attr", True)

# 添加控制依赖
builder.add_control_dependency(input_tensor, [const_float])

# 设置图输出
builder.set_graph_output(input_tensor, 0)

# 构建图
graph = builder.build_and_reset()

# 或在构建时直接指定输出
# graph = builder.build_and_reset(outputs=[input_tensor])
```
