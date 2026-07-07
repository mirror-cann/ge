# EsCreateConstFloat

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建float类型的Const节点。

## 函数原型

```c
EsCTensorHolder *EsCreateConstFloat(EsCGraphBuilder *graph, const float *value, const int64_t *dims, int64_t dim_num)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 图构建器指针。 |
| value | 输入 | 张量数据指针。 |
| dims | 输入 | 张量维度数组指针。 |
| dim_num | 输入 | 维度数量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 成功返回创建的tensor持有者指针，失败返回nullptr。 |

## 约束说明

无
