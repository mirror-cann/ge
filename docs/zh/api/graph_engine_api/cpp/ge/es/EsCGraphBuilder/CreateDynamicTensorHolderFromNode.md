# CreateDynamicTensorHolderFromNode

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

从图节点获取动态输出的张量。

## 函数原型

```c++
std::vector<EsCTensorHolder *> *CreateDynamicTensorHolderFromNode(const ge::GNode& node, int32_t start_idx, int32_t output_num)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 图节点。 |
| start_idx | 输入 | 输出索引起始点。 |
| output_num | 输入 | 总动态输出数量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::vector<EsCTensorHolder *>* | 动态输出容器持有者指针。 |

## 约束说明

无
