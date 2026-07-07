# CreateConstV2

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

使用ABI安全接口创建Const算子。

## 函数原型

```c++
template <typename T>
EsTensorHolder CreateConstV2(const std::vector<T> &value, const std::vector<int64_t> &dims, ge::DataType dt, ge::Format format = FORMAT_ND)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| value | 输入 | 张量数据向量。 |
| dims | 输入 | 张量维度向量。 |
| T | 输入 | 张量数据类型。 |
| dt | 输入 | 张量的数据类型。 |
| format | 输入 | 张量格式，默认为FORMAT_ND。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsTensorHolder | 返回创建的Const的张量持有者算子，失败时返回无效的EsTensorHolder。 |

## 约束说明

该接口使用CompliantNodeBuilder V2接口构造Const节点，避免旧IR定义结构体中std::string跨C++ ABI传递的风险。

使用该接口要求运行时软件包支持CompliantNodeBuilder V2符号。如果需要兼容不包含V2符号的老软件包，请使用[CreateConst](CreateConst.md)。

## 调用示例

```c++
EsGraphBuilder builder("test_graph");
std::vector<float> vecf = {1.1, 2.0, 3.2, 4.4};
std::vector<int64_t> dims = {4};
auto c1 = builder.CreateConstV2<float>(vecf, dims, ge::DT_FLOAT);
```
