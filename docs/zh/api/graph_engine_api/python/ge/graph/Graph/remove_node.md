# remove\_node

## 产品支持情况

全量芯片支持。

## 功能说明

从图中移除节点。

## 函数原型

```python
remove_node(node: Node) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| node | 输入 | 要移除的节点。 |

## 返回值说明

无

## 约束说明

- 如果node不是Node类型，抛出TypeError。
- 如果移除操作失败，抛出RuntimeError。
