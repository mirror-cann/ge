# NodeIo

## 产品支持情况

全量芯片支持。

## 功能说明

以冻结数据类形式描述一个节点输出锚点。

## 函数原型

```python
@dataclass(frozen=True)
class NodeIo:
    node: Node
    index: int = 0
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 节点对象，类型为ge.graph.Node。 |
| index | 输入 | 节点输出索引，默认为0。 |

## 返回值说明

无

## 约束说明

无
