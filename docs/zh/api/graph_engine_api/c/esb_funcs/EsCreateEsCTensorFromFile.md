# EsCreateEsCTensorFromFile

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

用于C用户通过binary文件创建EsCTensor。

## 函数原型

```c
EsCTensor *EsCreateEsCTensorFromFile(const char *data_file_path, const int64_t *dim, int64_t dim_num,C_DataType data_type, C_Format format)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| data_file_path | 输入 | 张量binary数据文件路径。 |
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
int64_t dims[] = {3};
auto es_tensor =
    EsCreateEsCTensorFromFile(file_path.c_str(), dims, 1, C_DT_INT64, C_FORMAT_ALL);
```
