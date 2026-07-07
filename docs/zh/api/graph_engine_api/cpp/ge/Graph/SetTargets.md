# SetTargets

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

设置Graph的结束节点列表。

在该列表中的算子需要被执行到，但它的输出不用返回给用户。

## 函数原型

```c++
Graph &SetTargets(const std::vector<Operator> &targets)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| targets | 输入 | 结束节点列表。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | [Graph](Graph.md)& | 返回调用者本身。 |

## 约束说明

该接口调用需要放在构图流程的最后一步，若在调用该接口后对Outputs或者Targets节点进行删除操作，可能导致异常。
