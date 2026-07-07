# CopyFrom

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

拷贝原图生成目标图。

目标图的graph\_name优先使用用户指定的，未指定则使用原图的graph\_name。graph\_id/session归属，拷贝后需要用户自行添加。

## 函数原型

```c++
graphStatus Graph::CopyFrom(const Graph &src_graph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| src_graph | 输入 | 待拷贝的原图。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
