# Execute

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/custom\_op.h\>
- 库文件：libgraph.so

## 功能说明

Eager模式下自定义算子的执行入口。

## 函数原型

```c++
virtual graphStatus Execute(gert::EagerOpExecutionContext *ctx)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| ctx | 输入 | 执行时上下文，可通过上下文获取input tensor，分配输出内存，分配workspace等。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功<br>其他值：失败 |

## 约束说明

无
