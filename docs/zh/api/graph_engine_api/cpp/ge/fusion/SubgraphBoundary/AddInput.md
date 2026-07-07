# AddInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler

## 功能说明

为子图边界添加一个输入。

## 函数原型

```c++
Status AddInput(int64_t index, SubgraphInput input)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 输入索引。 |
| input | 输入 | 输入tensor的描述。详见[SubgraphInput](../SubgraphInput/SubgraphInput.md)介绍。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功<br>FAILED：失败 |

## 约束说明

无
