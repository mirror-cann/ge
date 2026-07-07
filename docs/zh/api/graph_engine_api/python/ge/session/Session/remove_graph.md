# remove\_graph

## 功能说明

从会话中移除图。

## 函数原型

```python
remove_graph(graph_id: int) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| graph_id | 输入 | 图ID。 |

## 返回值说明

无

## 约束说明

- 如果graph\_id不是整数，抛出TypeError。
- 如果移除失败，抛出RuntimeError。
