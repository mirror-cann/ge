# CaptureTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern.h\>
- 库文件：libge\_compiler.so

## 功能说明

捕获Pattern中一个Tensor，从而在[MatchResult](../MatchResult/MatchResult.md)中可以按序获取。

捕获Node的output，用于后续在MatchResult中通过捕获时的索引，快速获取到Node output，用户可以从Match Result中拿到Tensor描述做校验。

## 函数原型

```c++
Pattern &CaptureTensor(const NodeIo &node_output)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node_output | 输入 | Tensor的来源，即来自某个Node的某个输出。<br>因node_output可唯一标定一个图中Tensor，因此常用node_output指代Tensor。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Pattern & | Pattern的引用，便于级联调用。 |

## 约束说明

无
