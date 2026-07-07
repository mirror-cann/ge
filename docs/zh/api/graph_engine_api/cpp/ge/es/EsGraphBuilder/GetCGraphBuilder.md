# GetCGraphBuilder

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取底层的C语言图构建器。

## 函数原型

```c++
EsCGraphBuilder *GetCGraphBuilder() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCGraphBuilder * | 返回C语言图构建器指针，返回值的生命周期由EsGraphBuilder管理。 |

## 约束说明

无
