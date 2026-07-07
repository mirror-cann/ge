# GetOutput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler

## 功能说明

根据输出索引，获取子图输出Tensor的描述。

## 函数原型

```c++
Status GetOutput(int64_t index, SubgraphOutput &subgraph_output) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 输出索引。 |
| subgraph_output | 输出 | 输出Tensor的描述。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：获取成功<br>FAILED：获取失败 |

## 约束说明

无
