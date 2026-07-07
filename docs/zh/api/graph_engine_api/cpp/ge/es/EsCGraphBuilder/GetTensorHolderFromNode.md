# GetTensorHolderFromNode

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

从图节点获取张量持有者。

## 函数原型

```c++
EsCTensorHolder *GetTensorHolderFromNode(const ge::GNode &node, int32_t output_index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 图节点。 |
| output_index | 输入 | 输出索引。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 张量持有者指针。 |

## 约束说明

无
