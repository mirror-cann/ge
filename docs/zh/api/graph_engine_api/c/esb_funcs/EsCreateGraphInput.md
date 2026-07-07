# EsCreateGraphInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建一个图上的Data输入。

## 函数原型

```c
EsCTensorHolder *EsCreateGraphInput(EsCGraphBuilder *graph, int64_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 图构建器指针。 |
| index | 输入 | 第几个图的输入，从0开始计数，节点命名为input_{index}。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 成功返回创建的tensor持有者指针，失败返回nullptr。 |

## 约束说明

无

## 调用示例

```c
// 1. 创建图构建器（EsCGraphBuilder）
EsCGraphBuilder *builder = EsCreateGraphBuilder("graph_name");
// 创建第一个输入节点（索引从0开始）
EsCTensorHolder *input0 = EsCreateGraphInput(builder, 0);
// 创建第二个输入节点
EsCTensorHolder *input1 = EsCreateGraphInput(builder, 1);
// 使用完后释放
EsDestroyGraphBuilder(builder);
```
