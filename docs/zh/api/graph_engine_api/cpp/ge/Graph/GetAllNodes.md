# GetAllNodes

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

获取图中的所有节点，包含本图及子图的节点。

## 函数原型

```c++
std::vector<GNode> GetAllNodes() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::vector\<GNode> | 图中的所有节点，按照系统拓扑排序的结果顺序进行输出。 |

## 约束说明

无
