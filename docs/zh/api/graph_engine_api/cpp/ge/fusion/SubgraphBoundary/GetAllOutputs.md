# GetAllOutputs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler

## 功能说明

获取子图所有输出Tensor的描述。

## 函数原型

```c++
Status GetAllOutputs(std::vector<SubgraphOutput> &subgraph_outputs) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| subgraph_outputs | 输出 | 子图所有输出Tensor的描述。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：获取成功<br>FAILED：获取失败 |

## 约束说明

无
