# ConstructFromInputs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

支持基于用户构造的Operator对象生成一个Graph对象。

功能与[SetInputs](SetInputs.md)一致。SetInputs未来会逐步消亡，统一使用此接口。

## 函数原型

```c++
static GraphPtr ConstructFromInputs(const std::vector<Operator> &inputs, const AscendString &name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| inputs | 输入 | 整图输入的Operator。 |
| name | 输入 | Graph的名字。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | GraphPtr | 图指针，返回新构造的图。 |

## 约束说明

无

## 调用示例

```c++
GraphPtr graph;
graph =  Graph::ConstructFromInputs(inputs, graph_name);
graph->SetOutputs(outputs);
```
