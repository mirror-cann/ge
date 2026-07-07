# GetAllInputs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler

## 功能说明

获取所有子图输入Tensor的描述。

## 函数原型

```c++
Status GetAllInputs(std::vector<SubgraphInput> &subgraph_input) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| subgraph_input | 输出 | 子图边界所有输入Tensor的描述。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：获取成功<br>FAILED：获取失败 |

## 约束说明

无
