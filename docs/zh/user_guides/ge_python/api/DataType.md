# DataType

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.graph import DataType
```

## 功能说明

DataType 是一个继承自 IntEnum 的枚举类，用于定义张量支持的所有元素数据类型。每个枚举值对应一个整数值，与底层 C++ 接口中的数据类型定义保持一致。

## 枚举值列表

| 枚举名 | 值 | 说明 |
| :----- | :--- | :--- |
| DT_FLOAT | 0 | 单精度浮点型（float32） |
| DT_FLOAT16 | 1 | 半精度浮点型（float16） |
| DT_INT8 | 2 | 8 位有符号整型 |
| DT_INT32 | 3 | 32 位有符号整型 |
| DT_UINT8 | 4 | 8 位无符号整型 |
| DT_INT16 | 6 | 16 位有符号整型 |
| DT_UINT16 | 7 | 16 位无符号整型 |
| DT_UINT32 | 8 | 32 位无符号整型 |
| DT_INT64 | 9 | 64 位有符号整型 |
| DT_UINT64 | 10 | 64 位无符号整型 |
| DT_DOUBLE | 11 | 双精度浮点型（float64） |
| DT_BOOL | 12 | 布尔型 |
| DT_STRING | 13 | 字符串类型 |
| DT_DUAL_SUB_INT8 | 14 | 双输出子类型 int8 |
| DT_DUAL_SUB_UINT8 | 15 | 双输出子类型 uint8 |
| DT_COMPLEX64 | 16 | 64 位复数型 |
| DT_COMPLEX128 | 17 | 128 位复数型 |
| DT_QINT8 | 18 | 量化 8 位有符号整型 |
| DT_QINT16 | 19 | 量化 16 位有符号整型 |
| DT_QINT32 | 20 | 量化 32 位有符号整型 |
| DT_QUINT8 | 21 | 量化 8 位无符号整型 |
| DT_QUINT16 | 22 | 量化 16 位无符号整型 |
| DT_RESOURCE | 23 | 资源类型 |
| DT_STRING_REF | 24 | 字符串引用类型 |
| DT_DUAL | 25 | 双输出类型 |
| DT_VARIANT | 26 | 变体类型 |
| DT_BF16 | 27 | BF16 浮点型 |
| DT_UNDEFINED | 28 | 未定义数据类型，用于指示 DataType 字段未被设置 |
| DT_INT4 | 29 | 4 位有符号整型 |
| DT_UINT1 | 30 | 1 位无符号整型 |
| DT_INT2 | 31 | 2 位有符号整型 |
| DT_UINT2 | 32 | 2 位无符号整型 |
| DT_COMPLEX32 | 33 | 32 位复数型 |
| DT_HIFLOAT8 | 34 | 8 位高精度浮点型 |
| DT_FLOAT8_E5M2 | 35 | 8 位浮点型（E5M2 格式） |
| DT_FLOAT8_E4M3FN | 36 | 8 位浮点型（E4M3FN 格式） |
| DT_FLOAT8_E8M0 | 37 | 8 位浮点型（E8M0 格式） |
| DT_FLOAT6_E3M2 | 38 | 6 位浮点型（E3M2 格式） |
| DT_FLOAT6_E2M3 | 39 | 6 位浮点型（E2M3 格式） |
| DT_FLOAT4_E2M1 | 40 | 4 位浮点型（E2M1 格式） |
| DT_FLOAT4_E1M2 | 41 | 4 位浮点型（E1M2 格式） |
| DT_HIFLOAT4 | 42 | 4 位高精度浮点型 |
| DT_MAX | 43 | 数据类型边界标记，不代表实际数据类型 |

## 约束说明

- DataType 继承自 IntEnum，因此可以与整数进行比较和运算。
- 枚举值 5 为保留值，未在当前版本中使用。
- DT_MAX 仅作为数据类型范围的边界标记，不应作为实际数据类型使用。
- DT_UNDEFINED 用于标识未设置的数据类型字段，不应在正常业务逻辑中赋值。

## 使用示例

```python
from ge.graph import DataType

# 使用枚举值
dtype = DataType.DT_FLOAT
print(dtype)        # DataType.DT_FLOAT
print(dtype.value)  # 0

# 枚举值比较
print(DataType.DT_FLOAT == 0)     # True
print(DataType.DT_FLOAT16.value)  # 1

# 配合 TensorDesc 使用
from ge.graph import TensorDesc, Format
desc = TensorDesc(shape=[1, 3, 224, 224], format=Format.FORMAT_NCHW, data_type=DataType.DT_FLOAT16)
```
