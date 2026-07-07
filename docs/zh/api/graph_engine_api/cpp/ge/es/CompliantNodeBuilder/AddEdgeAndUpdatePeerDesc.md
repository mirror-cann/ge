# AddEdgeAndUpdatePeerDesc

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

添加边并更新对向Tensor的描述信息。

## 函数原型

```c++
graphStatus AddEdgeAndUpdatePeerDesc(Graph &graph, GNode &src_node, int32_t src_port_index, GNode &dst_node, int32_t dst_port_index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 源节点src_node和目标节点dst_node所属图对象。 |
| src_node | 输入 | 源节点。 |
| src_port_index | 输入 | 源节点端口索引。 |
| dst_node | 输入 | 目标节点。 |
| dst_port_index | 输入 | 目标节点端口索引。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0): 成功<br>其他值: 失败 |

## 约束说明

无
