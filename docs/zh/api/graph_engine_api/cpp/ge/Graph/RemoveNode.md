# RemoveNode

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

删除图中的指定节点，并删除节点之间的连边。

## 函数原型

```c++
graphStatus RemoveNode(GNode &node)
graphStatus RemoveNode(GNode &node, bool contain_subgraph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 删除图中的指定节点，如果是子图中的节点，则需要指定contain_subgraph为true。 |
| contain_subgraph | 输入 | 删除的指定节点是否在子图中。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
