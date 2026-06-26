# Tensor

## Product Support

| Product | Support |
| :--- | :---: |
| Atlas A3 Training Products/Atlas A3 Inference Products | √ |
| Atlas A2 Training Products/Atlas A2 Inference Products | √ |

## Module Import

```python
from ge.graph import Tensor
```

## Functionality Description

Tensor class is tensor data class, supports creating tensors via memory data or files. Supports setting and getting tensor's Format, DataType, Shape, Data (TensorLike) and Placement. Tensors support migration between Host and Device.

## Function Prototype

### Constructor

```python
Tensor(data=None, file_path=None, data_type=DataType.DT_FLOAT, format=Format.FORMAT_ND, shape=None, placement=Placement.PLACEMENT_HOST)
```

### Properties

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

### Methods

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

## Parameter Description

### Constructor Parameters

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| data | Union[List[int], List[float], List[bool], None] | No | Memory data, pass tensor data via list. Choose one between data and file_path, cannot specify both. |
| file_path | Union[str, None] | No | File path, read tensor data from file. Choose one between file_path and data, cannot specify both. |
| data_type | DataType | No | Tensor's data type, use DataType enum value, default is DataType.DT_FLOAT. |
| format | Format | No | Tensor's data format, use Format enum value, default is Format.FORMAT_ND. |
| shape | Union[List[int], None] | No | Tensor's shape, represented as integer list for each dimension size. If None, indicates scalar. |
| placement | Placement | No | Tensor's placement location, use Placement enum value, default is Placement.PLACEMENT_HOST. |

### set_format Parameters

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| format | Format | Yes | Target data format, use Format enum value. |

### set_data_type Parameters

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| data_type | DataType | Yes | Target data type, use DataType enum value. |

## Return Value Description

### Constructor

Returns Tensor object instance.

### Property Return Values

| Property | Return Type | Description |
| :--- | :--- | :--- |
| format | Format | Tensor's data format. |
| data_type | DataType | Tensor's data type. |
| shape | Shape | Tensor's shape information. |
| data | TensorLike | Tensor's data content. Scalar returns single value, non-scalar returns nested list structure. |
| placement | Placement | Tensor's placement location. |

### Method Return Values

| Method | Return Type | Description |
| :--- | :--- | :--- |
| set_format | Tensor | Returns self, supports chained calls. |
| get_format | Format | Returns tensor's data format. |
| set_data_type | Tensor | Returns self, supports chained calls. |
| get_data_type | DataType | Returns tensor's data type. |
| get_shape | Shape | Returns tensor's shape information. |
| get_data | TensorLike | Returns tensor's data content. Scalar returns single value, non-scalar returns nested list structure. |
| get_tensor_desc | TensorDesc | Returns tensor's description info (TensorDesc object). |
| get_placement | Placement | Returns tensor's placement location. |
| to_host | Tensor | Returns self, migrates tensor from Device to Host. |
| to_device | Tensor | Returns self, migrates tensor from Host to Device. |

## Constraints

- When constructing tensor, only one of data and file_path can be specified, cannot specify both, and cannot specify neither (when neither specified creates empty tensor).
- Supported data types include: DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64, DT_BOOL. Does not support DT_DOUBLE.
- shape parameter must be integer list (list of int), if None indicates scalar.
- placement parameter must be Placement enum value.
- Tensor does not support copy and deepcopy.
- to_host() only applies to tensors currently on Device; to_device() only applies to tensors currently on Host.
