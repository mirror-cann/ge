# InferDataType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/custom\_op.h\>
- 库文件：libgraph.so

## 功能说明

数据类型推理函数，通过数据类型推理上下文获取输入张量数据类型信息，推导并设置输出张量的数据类型，用于图编译阶段的类型推导。

## 函数原型

```c++
virtual graphStatus InferDataType(gert::InferDataTypeContext *ctx) = 0
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ctx | 输入 | 数据类型推理上下文，可通过上下文获取输入张量数据类型，设置输出张量数据类型等。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
