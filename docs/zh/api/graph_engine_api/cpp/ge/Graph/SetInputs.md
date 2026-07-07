# SetInputs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

设置Graph内的输入算子。

## 函数原型

```c++
Graph &SetInputs(const std::vector<Operator> &inputs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| inputs | 输入 | Graph内的输入算子。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | [Graph](Graph.md)& | 返回调用者本身。 |

## 约束说明

无
