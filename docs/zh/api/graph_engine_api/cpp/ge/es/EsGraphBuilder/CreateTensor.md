# CreateTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建指定数据类型和张量形状的张量。

## 函数原型

```c++
template <typename T>
std::unique_ptr<Tensor> CreateTensor(const std::vector<T> &value, const std::vector<int64_t> &dims, DataType dt, Format format = FORMAT_ND);

template <typename T>
std::unique_ptr<Tensor> CreateTensor(const T *value, const int64_t *dims, int64_t dim_num, DataType dt, Format format = FORMAT_ND);
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| T | 输入 | 张量数据类型。 |
| value | 输入 | 张量数据指针。 |
| dims | 输入 | 张量维度数组指针。 |
| dim_num | 输入 | 张量维度数量。 |
| dt | 输入 | 张量的数据类型。 |
| format | 输入 | 张量格式，默认为FORMAT_ND。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::unique_ptr\<Tensor> | 返回创建的张量智能指针，失败时返回nullptr。 |

## 约束说明

无

## 调用示例

模版一：

```c++
EsGraphBuilder builder("graph_name");
std::vector<float> data = {1.0f, 2.0f, 3.0f};
std::vector<int64_t> dims = {3};
auto tensor = builder.CreateTensor<float>(data, dims, ge::DT_FLOAT);
```

模版二：

```c++
std::vector<float> data = {1.0f, 2.0f, 3.0f};
std::vector<int64_t> dims = {3};
auto tensor = ge::es::CreateTensor<float>(data.data(), dims.data(), dims.size(), ge::DT_FLOAT);
```
