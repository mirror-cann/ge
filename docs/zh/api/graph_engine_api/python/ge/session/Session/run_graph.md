# run\_graph

## 功能说明

运行图。

## 函数原型

```python
run_graph(graph_id: int, inputs: List[Tensor]) -> List[Tensor]
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| graph_id | 输入 | 图ID。 |
| inputs | 输入 | 输入张量列表。 |

## 返回值说明

\(List\[Tensor\]\)输出张量列表。

## 约束说明

- 如果graph\_id不是整数，抛出TypeError。
- 如果inputs中有元素不是Tensor类型，抛出TypeError。
- 如果运行失败，抛出RuntimeError。
- 更多约束请参见[约束说明](../../../../cpp/ge/Session/RunGraph.md#约束说明)。
