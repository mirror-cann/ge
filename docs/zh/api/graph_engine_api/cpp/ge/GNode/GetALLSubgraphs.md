# GetALLSubgraphs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/gnode.h\>
- 库文件：libgraph.so

## 功能说明

获取当前节点根图的所有子图对象。

## 函数原型

```c++
graphStatus GetALLSubgraphs(std::vector<GraphPtr> &graph_list) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_list | 输出 | 子图的指针，空表示无对应子图。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
