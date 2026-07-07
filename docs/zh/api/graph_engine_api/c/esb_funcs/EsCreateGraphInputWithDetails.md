# EsCreateGraphInputWithDetails

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建一个图上的输入。

## 函数原型

```c
EsCTensorHolder *EsCreateGraphInputWithDetails(EsCGraphBuilder *graph, int64_t index, const char *name, const char *type, C_DataType data_type, C_Format format, const int64_t *shape, int64_t dim_num);
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 图构建器指针。 |
| index | 输入 | 第几个图的输入，从0开始计数。 |
| name | 输入 | 输入的名字，如果为空则默认为input_{index}。 |
| type | 输入 | 输入的类型，如果为空则默认为Data。 |
| data_type | 输入 | 数据类型。 |
| format | 输入 | 格式类型。 |
| shape | 输入 | shape信息，shape如果为空则为标量。 |
| dim_num | 输入 | 维度数量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 返回的Tensor资源被图构建器管理。 |

## 约束说明

无

## 调用示例

```c
#include "c_types.h" //包含c的枚举类型定义
// 1. 创建图构建器（EsCGraphBuilder）
EsCGraphBuilder *builder = EsCreateGraphBuilder("graph_name");
// 2. 准备Shape(维度信息)
// 例如：Batch=1, Channel=3, Height=224, Width=224
int64_t input_shape[] = {1, 3, 224, 224};

// 计算维度数量(dim_num)
int64_t dim_num = sizeof(input_shape) / sizeof(input_shape[0]); // 结果为 4

// 3. 准备其他参数
int64_t input_index = 0;           // 第 0 个输入
const char *input_name = "data";   // 输入节点名称
const char *node_type = "Data";   // 输入的节点类型
C_DataType dtype = C_DT_FLOAT;   // 数据类型为 float
C_Format fmt = C_FORMAT_NCHW;         // 数据排布为 NCHW

// 4. 调用函数
EsCTensorHolder *input_tensor = EsCreateGraphInputWithDetails(
    builder,      // graph: 图构建器指针
    input_index,  // index: 输入索引
    input_name,   // name: 输入名称
    node_type,    // type: 节点类型字符串
    dtype,        // data_type: 数据类型枚举
    fmt,          // format: 格式枚举
    input_shape,  // shape: 维度数组指针
    dim_num       // dim_num: 维度数组的长度
);
```
