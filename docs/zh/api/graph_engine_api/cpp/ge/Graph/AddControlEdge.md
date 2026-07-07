# AddControlEdge

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

新增一个控制边。

## 函数原型

```c++
graphStatus AddControlEdge(GNode &src_node, GNode &dst_node)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| src_node | 输入 | 连接边的源节点。 |
| dst_node | 输入 | 连接边的目的节点。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
