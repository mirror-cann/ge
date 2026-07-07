# create\_input

## 产品支持情况

全量芯片支持。

## 功能说明

创建图输入。

## 函数原型

```python
create_input(index: int, *, name: Optional[str] = None, type_str: Optional[InputType] = InputType.DATA, data_type: Optional[DataType] = DataType.DT_FLOAT, format: Optional[Format] = Format.FORMAT_ND, shape: Optional[List[int]] = None) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| index | 输入 | 输入索引。 |
| name | 输入 | 可选输入，输入名称，默认为"input_{index}"。 |
| type_str | 输入 | 可选输入，输入类型，默认为DATA。详见[InputType](../../es/InputType.md)。 |
| data_type | 输入 | 可选输入，数据类型，默认为DT_FLOAT。详见[DataType](../../DataType.md)。 |
| format | 输入 | 可选输入，数据格式，默认为FORMAT_ND。详见[Format](../../Format.md)。 |
| shape | 输入 | 可选输入，形状维度列表，None表示标量。 |

## 返回值说明

\(TensorHolder\) 表示输入的张量持有者。

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果输入创建失败，抛出RuntimeError。
