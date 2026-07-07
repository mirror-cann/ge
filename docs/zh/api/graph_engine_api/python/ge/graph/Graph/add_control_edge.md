# add\_control\_edge

## 产品支持情况

全量芯片支持。

## 功能说明

添加控制边。

## 函数原型

```python
add_control_edge(src_node: Node, dst_node: Node) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| src_node | 输入 | 边的源节点。 |
| dst_node | 输入 | 边的目标节点。 |

## 返回值说明

无

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果添加操作失败，抛出RuntimeError。
