# CreateTensorFromFile

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

从binary文件创建指定数据类型和张量形状的张量。

## 函数原型

```c++
template <typename T>
std::unique_ptr<Tensor> CreateTensorFromFile(const char *data_file_path, const std::vector<int64_t> &dims, DataType dt, Format format = FORMAT_ND);
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| T | 输入 | 张量数据类型。 |
| data_file_path | 输入 | binary文件数据路径。 |
| dims | 输入 | 张量维度向量。 |
| dt | 输入 | 张量的数据类型。 |
| format | 输入 | 张量格式，默认为FORMAT_ND。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::unique_ptr\<Tensor> | 返回创建的张量智能指针，失败时返回nullptr。 |

## 约束说明

无

## 调用示例

```c++
std::vector<int64_t> dims = {1};
auto tensor = CreateTensorFromFile<int64_t>(file_path.c_str(), dims, ge::DT_INT64, ge::FORMAT_ALL);
```
