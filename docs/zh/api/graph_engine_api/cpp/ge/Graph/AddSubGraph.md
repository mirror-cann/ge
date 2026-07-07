# AddSubGraph

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

添加子图。

以子图的name为key，不允许出现重复。若添加name相同的子图，添加子图失败。

## 函数原型

```c++
graphStatus AddSubGraph(const Graph &subgraph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| subgraph | 输入 | 需要添加的子图实例。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS：子图添加成功<br>其他：子图添加失败 |

## 约束说明

无
