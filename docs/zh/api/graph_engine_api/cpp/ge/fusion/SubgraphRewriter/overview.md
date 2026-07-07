# 简介

一个用于改图的工具类。接收图上的一个范围，和一个替换图replacement，使用replacement将图上给定范围中的节点替换掉。

## 需要包含的头文件

```c++
#include <ge/fusion/graph_rewriter.h>
```

## Public成员函数

```c++
static Status Replace(const SubgraphBoundary &subgraph, const Graph &replacement)
static Status Replace(const SubgraphBoundary &subgraph, Graph &&replacement)
```
