# \_\_init\_\_

## 产品支持情况

全量芯片支持。

## 功能说明

构造SubgraphOutput。支持两种构造方式：构造空输出（随后调用set\_output设置），或直接绑定一个输出锚点。

## 函数原型

```python
__init__() -> None
__init__(node: Node, out_index: int) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 可选。主图节点，类型为ge.graph.Node。 |
| out_index | 输入 | 可选。节点输出索引。 |

## 返回值说明

无

## 约束说明

无
