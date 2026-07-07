# create\_replacement

## 产品支持情况

全量芯片支持。

## 功能说明

创建替换图，用于在模式融合或算子分解中提供替换子图。

## 函数原型

```python
create_replacement(graph: Graph) -> Graph
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 替换图，类型为ge.graph.Graph。 |

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| Graph | 返回传入的替换图对象。若输入不是ge.graph.Graph类型，将抛出TypeError。 |

## 约束说明

无
