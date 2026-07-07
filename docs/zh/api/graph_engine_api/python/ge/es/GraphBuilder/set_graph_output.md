# set\_graph\_output

## 产品支持情况

全量芯片支持。

## 功能说明

设置图输出。

## 函数原型

```python
set_graph_output(tensor: TensorHolder, output_index: int) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| tensor | 输入 | 要设置为输出的张量持有者。 |
| output_index | 输入 | 输出索引。 |

## 返回值说明

无

## 约束说明

如果参数类型不正确，抛出TypeError。
