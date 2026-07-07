# CreateBoolTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建bool数据类型和张量形状的张量。

## 函数原型

```c++
std::unique_ptr<Tensor> CreateBoolTensor(const std::vector<bool> &value, const std::vector<int64_t> &dims, Format format = FORMAT_ND)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| value | 输入 | 张量数据向量。 |
| dims | 输入 | 张量维度向量。 |
| format | 输入 | 张量格式，默认为FORMAT_ND。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::unique_ptr\<Tensor> | 返回创建的张量智能指针，失败时返回nullptr。 |

## 约束说明

无

## 调用示例

```c++
EsGraphBuilder builder("graph_name");
std::vector<bool> data = {true, false, true, false};
std::vector<int64_t> dims = {4};
auto tensor = builder.CreateBoolTensor(data,dims);
```
