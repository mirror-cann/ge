# RemoveEdge

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

删除图中的指定连接边。

## 函数原型

```c++
graphStatus RemoveEdge(GNode &src_node, const int32_t src_port_index, GNode &dst_node, const int32_t dst_port_index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| src_node | 输入 | 连接边的源节点。 |
| src_port_index | 输入 | 源节点的输出端口号(-1表示控制边)。 |
| dst_node | 输入 | 连接边的目的节点。 |
| dst_port_index | 输入 | 目的节点的输入端口号（-1表示控制边）。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
