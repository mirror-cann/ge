# EsSetFormat

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置Tensor的内存格式，不设置默认为ND。

## 函数原型

```c
uint32_t EsSetFormat(EsCTensorHolder *tensor, C_Format format)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | tensor持有者指针。 |
| format | 输入 | 格式类型。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | uint32_t | 成功为0，失败返回非0值。 |

## 约束说明

无

## 调用示例

```c
/ 1. 创建图构建器（EsCGraphBuilder）
EsCGraphBuilder *builder = EsCreateGraphBuilder("graph_name");
// 2. 添加起始节点
EsCTensorHolder *data0 = EsCreateInput(builder, 0); // 添加第 0 个输入节点, 默认为ND格式
EsSetFormat(data0, C_FORMAT_NCHW); //设置第0个输入节点的数据格式为NCHW
```
