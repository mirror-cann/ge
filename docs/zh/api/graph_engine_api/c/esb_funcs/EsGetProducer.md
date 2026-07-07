# EsGetProducer

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

本接口返回ge::GNode实例的快照。

## 函数原型

```c
EsCNode *EsGetProducer(EsCTensorHolder *tensor)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | Tensor持有者指针。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCNode * | 成功返回节点指针，失败返回nullptr |

## 约束说明

无

## 调用示例

```c
auto *tensor = EsCreateGraphInput(graph, 0);
auto *producer = static_cast<void *>(EsGetProducer(tensor));
```
