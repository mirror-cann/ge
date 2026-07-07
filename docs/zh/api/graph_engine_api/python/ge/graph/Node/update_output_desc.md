# update\_output\_desc

## 产品支持情况

全量芯片支持。

## 功能说明

更新指定输出端口的Tensor格式。

## 函数原型

```python
update_output_desc(index: int, tensor_desc: TensorDesc) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| index | 输入 | 算子的输出端口号索引。 |
| tensor_desc | 输入 | 需要更新的tensor格式。 |

## 返回值说明

无

## 约束说明

- 如果index不是整数，抛出TypeError。
- 如果tensor\_desc不是TensorDesc类型，抛出TypeError。
- 如果Tensor格式更新失败，抛出RuntimeError。
