# TensorDesc

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.graph import TensorDesc
```

## 功能说明

TensorDesc 类用于描述张量的元信息，包括形状（Shape）、数据格式（Format）和数据类型（DataType）。支持原始形状/格式和当前形状/格式的区分，适用于图构建过程中对张量属性的完整描述。所有 `set_*` 方法均返回 `self`，支持链式调用。不支持拷贝（`copy`）和深拷贝（`deepcopy`）。

## 类定义

```python
class TensorDesc:
    def __init__(
        self,
        shape: Optional[List[int]] = None,
        format: Optional[Format] = Format.FORMAT_ND,
        data_type: Optional[DataType] = DataType.DT_FLOAT,
    ) -> None
```

## 函数列表

| 函数 | 功能说明 |
| :--- | :--- |
| \_\_init\_\_(shape=None, format=Format.FORMAT_ND, data_type=DataType.DT_FLOAT) | 构造函数，创建 TensorDesc 对象。shape 为维度列表，None 表示标量；format 为数据格式，默认 FORMAT_ND；data_type 为数据类型，默认 DT_FLOAT |
| shape (property) | 以属性方式获取当前形状，返回 Shape 对象 |
| origin_shape (property) | 以属性方式获取原始形状，返回 Shape 对象 |
| format (property) | 以属性方式获取当前数据格式，返回 Format 枚举值 |
| origin_format (property) | 以属性方式获取原始数据格式，返回 Format 枚举值 |
| data_type (property) | 以属性方式获取数据类型，返回 DataType 枚举值 |
| get_shape() | 获取当前形状 |
| set_shape(shape) | 设置当前形状，支持链式调用 |
| get_origin_shape() | 获取原始形状 |
| set_origin_shape(shape) | 设置原始形状，支持链式调用 |
| get_format() | 获取当前数据格式 |
| set_format(format) | 设置当前数据格式，支持链式调用 |
| get_origin_format() | 获取原始数据格式 |
| set_origin_format(format) | 设置原始数据格式，支持链式调用 |
| get_data_type() | 获取数据类型 |
| set_data_type(data_type) | 设置数据类型，支持链式调用 |

## 参数说明

### \_\_init\_\_ 参数

| 参数名 | 类型 | 是否必选 | 说明 |
| :----- | :--- | :------: | :--- |
| shape | List[int] | 否 | 张量的维度列表，例如 [1, 3, 224, 224]。None 表示标量（空列表）。默认值为 None |
| format | Format | 否 | 张量的数据格式，取值为 Format 枚举值。默认值为 Format.FORMAT_ND |
| data_type | DataType | 否 | 张量的元素数据类型，取值为 DataType 枚举值。默认值为 DataType.DT_FLOAT |

### set_shape / set_origin_shape 参数

| 参数名 | 类型 | 是否必选 | 说明 |
| :----- | :--- | :------: | :--- |
| shape | List[int] | 是 | 目标维度列表，必须为整数列表 |

### set_format / set_origin_format 参数

| 参数名 | 类型 | 是否必选 | 说明 |
| :----- | :--- | :------: | :--- |
| format | Format | 是 | 目标数据格式，必须为 Format 枚举值 |

### set_data_type 参数

| 参数名 | 类型 | 是否必选 | 说明 |
| :----- | :--- | :------: | :--- |
| data_type | DataType | 是 | 目标数据类型，必须为 DataType 枚举值 |

## 返回值说明

| 函数 | 返回值类型 | 说明 |
| :--- | :--- | :--- |
| shape (property) | Shape | 当前形状对象 |
| origin_shape (property) | Shape | 原始形状对象 |
| format (property) | Format | 当前数据格式枚举值 |
| origin_format (property) | Format | 原始数据格式枚举值 |
| data_type (property) | DataType | 数据类型枚举值 |
| get_shape() | Shape | 当前形状对象 |
| set_shape(shape) | TensorDesc | 返回自身，支持链式调用 |
| get_origin_shape() | Shape | 原始形状对象 |
| set_origin_shape(shape) | TensorDesc | 返回自身，支持链式调用 |
| get_format() | Format | 当前数据格式枚举值 |
| set_format(format) | TensorDesc | 返回自身，支持链式调用 |
| get_origin_format() | Format | 原始数据格式枚举值 |
| set_origin_format(format) | TensorDesc | 返回自身，支持链式调用 |
| get_data_type() | DataType | 数据类型枚举值 |
| set_data_type(data_type) | TensorDesc | 返回自身，支持链式调用 |

## 约束说明

- 不支持拷贝操作：调用 `copy.copy()` 会抛出 RuntimeError。
- 不支持深拷贝操作：调用 `copy.deepcopy()` 会抛出 RuntimeError。
- 构造函数中，format 参数必须为 Format 枚举类型，否则抛出 TypeError。
- 构造函数中，data_type 参数必须为 DataType 枚举类型，否则抛出 TypeError。
- set_shape、set_origin_shape 的 shape 参数必须为整数列表（list of int），否则抛出 TypeError。
- set_format、set_origin_format 的 format 参数必须为 Format 枚举类型，否则抛出 TypeError。
- set_data_type 的 data_type 参数必须为 DataType 枚举类型，否则抛出 TypeError。
- 若底层 C API 调用失败，各方法将抛出 RuntimeError。

## 使用示例

```python
from ge.graph import TensorDesc, Shape, Format, DataType

# 创建 TensorDesc 对象
desc = TensorDesc(shape=[1, 3, 224, 224], format=Format.FORMAT_NCHW, data_type=DataType.DT_FLOAT)

# 通过属性获取张量信息
print(desc.shape)       # [1, 3, 224, 224]
print(desc.format)      # Format.FORMAT_NCHW
print(desc.data_type)   # DataType.DT_FLOAT

# 通过方法获取张量信息
shape = desc.get_shape()
fmt = desc.get_format()
dtype = desc.get_data_type()

# 链式调用设置属性
desc.set_shape([2, 3]).set_data_type(DataType.DT_INT32).set_format(Format.FORMAT_ND)
```
