# EsCreateScalarInt32

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建int32类型的标量常量。

## 函数原型

```c
EsCTensorHolder *EsCreateScalarInt32(EsCGraphBuilder *graph, int32_t value)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 图构建器指针。 |
| value | 输入 | 标量值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 成功返回创建的Tensor持有者指针，失败返回nullptr。 |

## 约束说明

无
