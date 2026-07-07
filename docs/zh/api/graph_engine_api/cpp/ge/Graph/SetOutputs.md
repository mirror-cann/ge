# SetOutputs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

设置Graph关联的输出算子。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
Graph &SetOutputs(const std::vector<Operator>& outputs)
Graph &SetOutputs(const std::vector<std::pair<Operator, std::vector<size_t>>> &output_indexs)
Graph &SetOutputs(const std::vector<std::pair<ge::Operator, std::string> > &outputs)
Graph &SetOutputs(const std::vector<std::pair<ge::Operator, AscendString>> &outputs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| outputs | 输入 | 输出节点列表，如果输出节点中有多个Output，则表示所有Output均返回结果，结果返回顺序按照此接口入参算子列表顺序，多个Output按照端口index从小到大排序。 |
| output_indexs | 输入 | 按照用户指定的算子及其Output（端口index描述，size_t类型）列表返回计算结果。顺序与此接口入参顺序一致。 |
| outputs | 输入 | 按照用户指定的算子及其Output（端口名字描述，string类型）列表返回计算结果。顺序与此接口入参顺序一致。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | [Graph](Graph.md)& | 返回调用者本身。 |

## 约束说明

该接口调用需要放在构图流程的最后一步，若在调用该接口后对Outputs或者Targets节点进行删除操作，可能导致异常。
