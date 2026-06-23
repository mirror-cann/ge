# TensorDesc

## Product Support

| Product | Support |
| :----------- | :------: |
| Atlas A3 Training Products/Atlas A3 Inference Products | √ |
| Atlas A2 Training Products/Atlas A2 Inference Products | √ |

## Module Import

```python
from ge.graph import TensorDesc
```

## Functionality Description

TensorDesc class describes tensor metadata, including Shape, Format and DataType. Supports distinguishing between original shape/format and current shape/format, suitable for complete description of tensor attributes during graph building. All `set_*` methods return `self`, supporting chained calls. Does not support copy and deepcopy.

## Class Definition

```python
class TensorDesc:
    def __init__(
        self,
        shape: Optional[List[int]] = None,
        format: Optional[Format] = Format.FORMAT_ND,
        data_type: Optional[DataType] = DataType.DT_FLOAT,
    ) -> None
```

## Function List

| Function | Description |
| :--- | :--- |
| \_\_init\_\_(shape=None, format=Format.FORMAT_ND, data_type=DataType.DT_FLOAT) | Constructor, creates TensorDesc object. shape is dimension list, None indicates scalar; format is data format, default FORMAT_ND; data_type is data type, default DT_FLOAT |
| shape (property) | Get current shape as property, returns Shape object |
| origin_shape (property) | Get original shape as property, returns Shape object |
| format (property) | Get current data format as property, returns Format enum value |
| origin_format (property) | Get original data format as property, returns Format enum value |
| data_type (property) | Get data type as property, returns DataType enum value |
| get_shape() | Get current shape |
| set_shape(shape) | Set current shape, supports chained calls |
| get_origin_shape() | Get original shape |
| set_origin_shape(shape) | Set original shape, supports chained calls |
| get_format() | Get current data format |
| set_format(format) | Set current data format, supports chained calls |
| get_origin_format() | Get original data format |
| set_origin_format(format) | Set original data format, supports chained calls |
| get_data_type() | Get data type |
| set_data_type(data_type) | Set data type, supports chained calls |

## Parameter Description

### \_\_init\_\_ Parameters

| Parameter | Type | Required | Description |
| :----- | :--- | :------: | :--- |
| shape | List[int] | No | Tensor's dimension list, e.g. [1, 3, 224, 224]. None indicates scalar (empty list). Default is None |
| format | Format | No | Tensor's data format, takes Format enum value. Default is Format.FORMAT_ND |
| data_type | DataType | No | Tensor's element data type, takes DataType enum value. Default is DataType.DT_FLOAT |

### set_shape / set_origin_shape Parameters

| Parameter | Type | Required | Description |
| :----- | :--- | :------: | :--- |
| shape | List[int] | Yes | Target dimension list, must be integer list |

### set_format / set_origin_format Parameters

| Parameter | Type | Required | Description |
| :----- | :--- | :------: | :--- |
| format | Format | Yes | Target data format, must be Format enum value |

### set_data_type Parameters

| Parameter | Type | Required | Description |
| :----- | :--- | :------: | :--- |
| data_type | DataType | Yes | Target data type, must be DataType enum value |

## Return Value Description

| Function | Return Type | Description |
| :--- | :--- | :--- |
| shape (property) | Shape | Current shape object |
| origin_shape (property) | Shape | Original shape object |
| format (property) | Format | Current data format enum value |
| origin_format (property) | Format | Original data format enum value |
| data_type (property) | DataType | Data type enum value |
| get_shape() | Shape | Current shape object |
| set_shape(shape) | TensorDesc | Returns self, supports chained calls |
| get_origin_shape() | Shape | Original shape object |
| set_origin_shape(shape) | TensorDesc | Returns self, supports chained calls |
| get_format() | Format | Current data format enum value |
| set_format(format) | TensorDesc | Returns self, supports chained calls |
| get_origin_format() | Format | Original data format enum value |
| set_origin_format(format) | TensorDesc | Returns self, supports chained calls |
| get_data_type() | DataType | Data type enum value |
| set_data_type(data_type) | TensorDesc | Returns self, supports chained calls |

## Constraints

- Does not support copy: calling `copy.copy()` will throw RuntimeError.
- Does not support deepcopy: calling `copy.deepcopy()` will throw RuntimeError.
- In constructor, format parameter must be Format enum type, otherwise throws TypeError.
- In constructor, data_type parameter must be DataType enum type, otherwise throws TypeError.
- For set_shape, set_origin_shape, shape parameter must be integer list (list of int), otherwise throws TypeError.
- For set_format, set_origin_format, format parameter must be Format enum type, otherwise throws TypeError.
- For set_data_type, data_type parameter must be DataType enum type, otherwise throws TypeError.
- If underlying C API call fails, methods will throw RuntimeError.

## Usage Example

```python
from ge.graph import TensorDesc, Shape, Format, DataType

# Create TensorDesc object
desc = TensorDesc(shape=[1, 3, 224, 224], format=Format.FORMAT_NCHW, data_type=DataType.DT_FLOAT)

# Get tensor info via properties
print(desc.shape)       # [1, 3, 224, 224]
print(desc.format)      # Format.FORMAT_NCHW
print(desc.data_type)   # DataType.DT_FLOAT

# Get tensor info via methods
shape = desc.get_shape()
fmt = desc.get_format()
dtype = desc.get_data_type()

# Chain call to set properties
desc.set_shape([2, 3]).set_data_type(DataType.DT_INT32).set_format(Format.FORMAT_ND)
```