# EsBuildGraphAndReset

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

本接口返回ge::Graph实例的指针，所有权转移给调用方。

## 函数原型

```c
EsCGraph *EsBuildGraphAndReset(EsCGraphBuilder *graph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 图构建器指针。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCGraph * | 成功返回图指针，失败返回nullptr。所有权转移给调用方，调用后图构建器将不再可用。 |

## 约束说明

无

## 调用示例

```c
EsCGraphBuilder *graph = EsCreateGraphBuilder("test_graph");
auto *tensor = EsCreateGraphInput(graph, 0);
EsCGraph *build_graph = EsBuildGraphAndReset(graph);
```
