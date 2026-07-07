# infer\_shape

## 产品支持情况

全量芯片支持。

## 功能说明

给定输入shape，对传入的Graph做全图shape推导。

本接口只做shape推导，不对图做任何其他优化（如常量折叠、死边消除等）。

## 函数原型

```python
infer_shape(graph: Graph, input_shapes: List[List[int]]) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| graph | 输入 | 待推导的图。 |
| input_shapes | 输入 | 图输入对应的shape。 |

## 返回值说明

无

## 约束说明

- 如果input\_shapes不是整数列表的列表，抛出TypeError。
- 如果shape推导失败，抛出RuntimeError。
