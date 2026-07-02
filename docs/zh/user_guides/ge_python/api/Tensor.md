# Tensor

## 产品支持情况

| 产品 | 是否支持 |
| :--- | :---: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.graph import Tensor
```

## 功能说明

Tensor 类是张量数据类，支持通过内存数据或文件创建张量。支持设置和获取张量的格式（Format）、数据类型（DataType）、形状（Shape）、数据（TensorLike）以及放置位置（Placement）。张量支持在 Host 和 Device 之间迁移。

## 函数原型

### 构造函数

```python
Tensor(data=None, file_path=None, data_type=DataType.DT_FLOAT, format=Format.FORMAT_ND, shape=None, placement=Placement.PLACEMENT_HOST)
```

### 属性

```python
@property
format -> Format
```

```python
@property
data_type -> DataType
```

```python
@property
shape -> Shape
```

```python
@property
data -> TensorLike
```

```python
@property
placement -> Placement
```

### 方法

```python
set_format(format: Format) -> Tensor
```

```python
get_format() -> Format
```

```python
set_data_type(data_type: DataType) -> Tensor
```

```python
get_data_type() -> DataType
```

```python
get_shape() -> Shape
```

```python
get_data() -> TensorLike
```

```python
get_tensor_desc() -> TensorDesc
```

```python
get_placement() -> Placement
```

```python
to_host() -> Tensor
```

```python
to_device() -> Tensor
```

## 参数说明

### 构造函数参数

| 参数名 | 类型 | 是否必选 | 说明 |
| :--- | :--- | :---: | :--- |
| data | Union[List[int], List[float], List[bool], None] | 否 | 内存数据，通过列表传入张量数据。与 file_path 二选一，不能同时指定。 |
| file_path | Union[str, None] | 否 | 文件路径，从文件读取张量数据。与 data 二选一，不能同时指定。 |
| data_type | DataType | 否 | 张量的数据类型，使用 DataType 枚举值，默认值为 DataType.DT_FLOAT。 |
| format | Format | 否 | 张量的数据格式，使用 Format 枚举值，默认值为 Format.FORMAT_ND。 |
| shape | Union[List[int], None] | 否 | 张量的形状，用整数列表表示各维度大小。若为 None，表示标量。 |
| placement | Placement | 否 | 张量的放置位置，使用 Placement 枚举值，默认值为 Placement.PLACEMENT_HOST。 |

### set_format 参数

| 参数名 | 类型 | 是否必选 | 说明 |
| :--- | :--- | :---: | :--- |
| format | Format | 是 | 目标数据格式，使用 Format 枚举值。 |

### set_data_type 参数

| 参数名 | 类型 | 是否必选 | 说明 |
| :--- | :--- | :---: | :--- |
| data_type | DataType | 是 | 目标数据类型，使用 DataType 枚举值。 |

## 返回值说明

### 构造函数

返回 Tensor 对象实例。

### 属性返回值

| 属性 | 返回类型 | 说明 |
| :--- | :--- | :--- |
| format | Format | 张量的数据格式。 |
| data_type | DataType | 张量的数据类型。 |
| shape | Shape | 张量的形状信息。 |
| data | TensorLike | 张量的数据内容。标量返回单个数值，非标量返回嵌套列表结构。 |
| placement | Placement | 张量的放置位置。 |

### 方法返回值

| 方法 | 返回类型 | 说明 |
| :--- | :--- | :--- |
| set_format | Tensor | 返回自身，支持链式调用。 |
| get_format | Format | 返回张量的数据格式。 |
| set_data_type | Tensor | 返回自身，支持链式调用。 |
| get_data_type | DataType | 返回张量的数据类型。 |
| get_shape | Shape | 返回张量的形状信息。 |
| get_data | TensorLike | 返回张量的数据内容。标量返回单个数值，非标量返回嵌套列表结构。 |
| get_tensor_desc | TensorDesc | 返回张量的描述信息（TensorDesc 对象）。 |
| get_placement | Placement | 返回张量的放置位置。 |
| to_host | Tensor | 返回自身，将张量从 Device 迁移到 Host。 |
| to_device | Tensor | 返回自身，将张量从 Host 迁移到 Device。 |

## 约束说明

- 构造张量时，data 和 file_path 只能指定其中一个，不能同时指定，也不能都不指定（都不指定时创建空张量）。
- 支持的数据类型包括：DT_FLOAT、DT_FLOAT16、DT_INT8、DT_INT32、DT_UINT8、DT_INT16、DT_UINT16、DT_UINT32、DT_INT64、DT_UINT64、DT_BOOL。不支持 DT_DOUBLE。
- shape 参数必须为整数列表（list of int），若为 None 则表示标量。
- placement 参数必须为 Placement 枚举值。
- Tensor 不支持拷贝（copy）和深拷贝（deepcopy）。
- to_host() 仅适用于当前位于 Device 的张量；to_device() 仅适用于当前位于 Host 的张量。
