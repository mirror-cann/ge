# EsCreateConstV2

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

使用ABI安全接口创建指定类型的Const算子（原始指针版本）。

## 函数原型

```c++
template <typename T>
EsCTensorHolder *EsCreateConstV2(EsCGraphBuilder *graph, const T *value, const int64_t *dims, int64_t dim_num, ge::DataType dt, ge::Format format = FORMAT_ND)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| T | 输入 | 张量数据类型。 |
| graph | 输入 | 图构建器指针。 |
| value | 输入 | 张量数据指针。 |
| dims | 输入 | 张量维度数组指针。 |
| dim_num | 输入 | 维度数量。 |
| dt | 输入 | 张量的数据类型。 |
| format | 输入 | 张量格式，默认为FORMAT_ND。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 返回创建的Const的张量持有者算子，失败时返回nullptr。 |

## 约束说明

该接口使用CompliantNodeBuilder V2接口构造Const节点，避免旧IR定义结构体中std::string跨C++ ABI传递的风险。

使用该接口要求运行时软件包支持CompliantNodeBuilder V2符号。如果需要兼容不包含V2符号的老软件包，请使用[EsCreateConst](EsCreateConst.md)。

## 调用示例

```c++
EsCGraphBuilder *graph = EsCreateGraphBuilder("graph_name");
std::vector<int64_t> data = {1, 2, 3};
std::vector<int64_t> dims = {3};
auto const_tensor = ge::es::EsCreateConstV2<int64_t>(graph, data.data(), dims.data(), dims.size(), ge::DT_INT64);
```
