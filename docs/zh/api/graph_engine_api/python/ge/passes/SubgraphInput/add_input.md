# add\_input

## 产品支持情况

全量芯片支持。

## 功能说明

追加一个输入锚点（节点输出索引）。

## 函数原型

```python
add_input(node: Node, out_index: int) -> int
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 主图节点，类型为ge.graph.Node。 |
| out_index | 输入 | 节点输出索引。 |

## 返回值说明

返回内部索引。

## 约束说明

无
