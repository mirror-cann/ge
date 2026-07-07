# AddGraphInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

在指定位置添加图输入。

## 函数原型

```c++
EsCTensorHolder *AddGraphInput(int32_t index, const ge::char_t *name = nullptr, const ge::char_t *type = nullptr, C_DataType data_type = C_DT_FLOAT, C_Format format = C_FORMAT_ND, const int64_t *dims = nullptr, int64_t dim_num = 0)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 输入索引位置。 |
| name | 输入 | 输入名称，可选。 |
| type | 输入 | 输入类型，可选。 |
| data_type | 输入 | 数据类型，默认为C_DT_FLOAT。 |
| format | 输入 | 数据格式，默认为C_FORMAT_ND。 |
| dims | 输入 | 维度数组，可选。 |
| dim_num | 输入 | 维度数量，默认为0。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 张量持有者指针。 |

## 约束说明

无
