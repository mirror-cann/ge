# EsSetShape

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置Tensor的shape，不设置默认为\[\]，也就是标量。

## 函数原型

```c
uint32_t EsSetShape(EsCTensorHolder *tensor, const int64_t *shape, int64_t dim_num)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | 持有者指针。 |
| shape | 输入 | 数组指针。 |
| dim_num | 输入 | 数组的维度数量。 |

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
EsCTensorHolder *data0 = EsCreateInput(builder, 0); // 添加第 0 个输入节点, 默认为[]标量
int64_t input_shape[] = {1, 3, 224, 224};
// 计算维度数量 (dim_num)
int64_t dim_num = sizeof(input_shape) / sizeof(input_shape[0]); // 结果为 4
EsSetShape(data0, input_shape, dim_num); //设置第0个输入节点shape
```
