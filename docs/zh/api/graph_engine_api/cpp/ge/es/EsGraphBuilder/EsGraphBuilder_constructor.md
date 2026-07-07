# EsGraphBuilder构造函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

构造函数，创建指定名称的图构建器。

## 函数原型

```c++
explicit EsGraphBuilder(const char *name) : graph_builder_(EsCreateGraphBuilder(name), EsDestroyGraphBuilder) {}
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 构建的图名称，如果为nullptr则使用默认名称"graph"。 |

## 返回值说明

无

## 约束说明

无

## 调用示例

```c++
// 1. 创建图构建器（EsGraphBuilder）
EsGraphBuilder builder("graph_name");
```
