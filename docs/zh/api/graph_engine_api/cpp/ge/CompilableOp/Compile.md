# Compile

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/custom\_op.h\>
- 库文件：libgraph.so

## 功能说明

自定义算子的编译回调接口，当算子进入GE构图编译流程时被调用，用于完成算子的编译相关处理（如生成编译产物、准备kernel等），通过编译上下文获取编译时信息。

## 函数原型

```c++
virtual graphStatus Compile(gert::OpCompileContext *ctx) = 0
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ctx | 输入 | 算子编译上下文。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功<br>其他值：失败 |

## 约束说明

无
