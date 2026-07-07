# AddOutput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler

## 功能说明

为子图边界添加一个输出。

## 函数原型

```c++
Status AddOutput(int64_t index, SubgraphOutput output)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 输出索引。 |
| output | 输入 | 输出Tensor的描述。详见[SubgraphOutput](../SubgraphOutput/SubgraphOutput.md)。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功<br>FAILED：失败 |

## 约束说明

无
