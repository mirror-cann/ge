# TensorDesc构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

创建一个新的TensorDesc对象。

## 函数原型

```python
TensorDesc(shape: Optional[List[int]] = None, format: Format = Format.FORMAT_ND, data_type: DataType = DataType.DT_FLOAT)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| shape | 输入 | 张量形状，类型为list[int]，可传入Shape对象。None表示标量。 |
| format | 输入 | 张量数据格式，类型为Format，默认值为Format.FORMAT_ND。 |
| data_type | 输入 | 张量数据类型，类型为DataType，默认值为DataType.DT_FLOAT。 |

## 返回值说明

TensorDesc对象。

## 约束说明

- 如果format不是Format类型，抛出TypeError。
- 如果data\_type不是DataType类型，抛出TypeError。
