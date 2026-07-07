# EsDestroyGraphBuilder

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

释放图构建器资源。

## 函数原型

```c
void EsDestroyGraphBuilder(EsCGraphBuilder *graph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 传入图构建器。 |

## 约束说明

无

## 调用示例

```c
// 创建图构建器（EsCGraphBuilder）
EsCGraphBuilder *builder = EsCreateGraphBuilder("graph_name");
// 省略构图代码...
// 释放builder及其管理的过程资源
EsDestroyGraphBuilder(builder);
```
