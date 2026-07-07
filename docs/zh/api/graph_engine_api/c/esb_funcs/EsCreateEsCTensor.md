# EsCreateEsCTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

本接口用于C用户创建EsCTensor。

## 函数原型

```c
EsCTensor *EsCreateEsCTensor(const void *data, const int64_t *dim, int64_t dim_num, C_DataType data_type,C_Format format)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| data | 输入 | 张量数据指针。 |
| dim | 输入 | 张量维度数组指针。 |
| dim_num | 输入 | 张量维度数量。 |
| data_type | 输入 | 张量的DataType枚举值。 |
| format | 输入 | 张量格式。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensor * | 张量的匿名指针，失败时返回nullptr。 |

## 约束说明

无

## 调用示例

```c
std::vector<float> data = {5.0, 6.1};
int64_t dims[] = {2};
auto tmp = data.data();
auto es_tensor = EsCreateEsCTensor(tmp, dims, 1, C_DT_FLOAT, C_FORMAT_ND);
```
