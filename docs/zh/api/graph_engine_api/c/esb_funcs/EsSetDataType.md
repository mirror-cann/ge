# EsSetDataType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置Tensor的数据类型，不设置默认为float。

## 函数原型

```c
uint32_t EsSetDataType(EsCTensorHolder *tensor, C_DataType data_type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | tensor持有者指针。 |
| data_type | 输入 | 数据类型。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | uint32_t | 成功为0，其他失败。 |

## 约束说明

无

## 调用示例

```c
/ 1. 创建图构建器（EsCGraphBuilder）
EsCGraphBuilder *builder = EsCreateGraphBuilder("graph_name");
// 2. 添加起始节点
EsCTensorHolder *data0 = EsCreateInput(builder, 0); // 添加第 0 个输入节点, 默认为FLOAT类型
EsSetDataType(data0, C_DT_INT32); //设置第0个输入节点的数据类型
```
