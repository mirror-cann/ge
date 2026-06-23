# DataType

## Product Support Status

| Product | Support Status |
| | :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.graph import DataType
```

## Functionality Description

DataType is an enumeration class inheriting from IntEnum, used to define all element data types supported by tensors. Each enumeration value corresponds to an integer value, consistent with data type definitions in the underlying C++ interface.

## Enumeration Value List

| Enumeration Name | Value | Description |
| | :----- | :--- | :--- |
| DT_FLOAT | 0 | Single-precision floating-point (float32) |
| DT_FLOAT16 | 1 | Half-precision floating-point (float16) |
| DT_INT8 | 2 | 8-bit signed integer |
| DT_INT32 | 3 | 32-bit signed integer |
| DT_UINT8 | 4 | 8-bit unsigned integer |
| DT_INT16 | 6 | 16-bit signed integer |
| DT_UINT16 | 7 | 16-bit unsigned integer |
| DT_UINT32 | 8 | 32-bit unsigned integer |
| DT_INT64 | 9 | 64-bit signed integer |
| DT_UINT64 | 10 | 64-bit unsigned integer |
| DT_DOUBLE | 11 | Double-precision floating-point (float64) |
| DT_BOOL | 12 | Boolean type |
| DT_STRING | 13 | String type |
| DT_DUAL_SUB_INT8 | 14 | Dual output sub-type int8 |
| DT_DUAL_SUB_UINT8 | 15 | Dual output sub-type uint8 |
| DT_COMPLEX64 | 16 | 64-bit complex number |
| DT_COMPLEX128 | 17 | 128-bit complex number |
| DT_QINT8 | 18 | Quantized 8-bit signed integer |
| DT_QINT16 | 19 | Quantized 16-bit signed integer |
| DT_QINT32 | 20 | Quantized 32-bit signed integer |
| DT_QUINT8 | 21 | Quantized 8-bit unsigned integer |
| DT_QUINT16 | 22 | Quantized 16-bit unsigned integer |
| DT_RESOURCE | 23 | Resource type |
| DT_STRING_REF | 24 | String reference type |
| DT_DUAL | 25 | Dual output type |
| DT_VARIANT | 26 | Variant type |
| DT_BF16 | 27 | BF16 floating-point type |
| DT_UNDEFINED | 28 | Undefined data type, used to indicate DataType field not set |
| DT_INT4 | 29 | 4-bit signed integer |
| DT_UINT1 | 30 | 1-bit unsigned integer |
| DT_INT2 | 31 | 2-bit signed integer |
| DT_UINT2 | 32 | 2-bit unsigned integer |
| DT_COMPLEX32 | 33 | 32-bit complex number |
| DT_HIFLOAT8 | 34 | 8-bit high-precision floating-point |
| DT_FLOAT8_E5M2 | 35 | 8-bit floating-point (E5M2 format) |
| DT_FLOAT8_E4M3FN | 36 | 8-bit floating-point (E4M3FN format) |
| DT_FLOAT8_E8M0 | 37 | 8-bit floating-point (E8M0 format) |
| DT_FLOAT6_E3M2 | 38 | 6-bit floating-point (E3M2 format) |
| DT_FLOAT6_E2M3 | 39 | 6-bit floating-point (E2M3 format) |
| DT_FLOAT4_E2M1 | 40 | 4-bit floating-point (E2M1 format) |
| DT_FLOAT4_E1M2 | 41 | 4-bit floating-point (E1M2 format) |
| DT_HIFLOAT4 | 42 | 4-bit high-precision floating-point |
| DT_MAX | 43 | Data type boundary marker, not representing actual data type |

## Constraint Description

- DataType inherits from IntEnum, therefore can be compared and operated with integers.
- Enumeration value 5 is a reserved value, not used in current version.
- DT_MAX is only used as a boundary marker for data type range, should not be used as actual data type.
- DT_UNDEFINED is used to identify unset data type fields, should not be assigned in normal business logic.

## Usage Example

```python
from ge.graph import DataType

# Using enumeration values
dtype = DataType.DT_FLOAT
print(dtype)        # DataType.DT_FLOAT
print(dtype.value)  # 0

# Enumeration value comparison
print(DataType.DT_FLOAT == 0)     # True
print(DataType.DT_FLOAT16.value)  # 1

# Using with TensorDesc
from ge.graph import TensorDesc, Format
desc = TensorDesc(shape=[1, 3, 224, 224], format=Format.FORMAT_NCHW, data_type=DataType.DT_FLOAT16)
```