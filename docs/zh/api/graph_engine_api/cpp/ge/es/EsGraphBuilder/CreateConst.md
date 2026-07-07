# CreateConst

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建Const算子。

## 函数原型

- 创建int64类型的Const算子

    ```c++
    EsTensorHolder CreateConst(const std::vector<int64_t> &value, const std::vector<int64_t> dims)
    ```

- 创建int32类型的Const算子

    ```c++
    EsTensorHolder CreateConst(const std::vector<int32_t> &value, const std::vector<int64_t> dims)
    ```

- 创建uint64类型的Const算子

    ```c++
    EsTensorHolder CreateConst(const std::vector<uint64_t> &value, const std::vector<int64_t> dims)
    ```

- 创建uint32类型的Const算子

    ```c++
    EsTensorHolder CreateConst(const std::vector<uint32_t> &value, const std::vector<int64_t> dims)
    ```

- 创建float类型的Const算子

    ```c++
    EsTensorHolder CreateConst(const std::vector<float> &value, const std::vector<int64_t> dims)
    ```

- 创建指定类型、格式和维度的Const算子（通用模板方法，该原型后续会废弃，建议使用[CreateConstV2](CreateConstV2.md)）

    ```c++
    template <typename T>
    EsTensorHolder CreateConst(const std::vector<T> &value, const std::vector<int64_t> &dims, ge::DataType dt,ge::Format format = FORMAT_ND)
    ```

    CreateConst通用模板方法内部使用IrDefOutputs/IrDefAttrs接口，IR定义结构体中包含std::string字段，不保证跨不同C++ ABI配置的兼容性。该接口保留用于兼容老软件包。

    需要ABI安全时，请使用[CreateConstV2](CreateConstV2.md)，CreateConstV2要求运行时软件包支持CompliantNodeBuilder V2接口。

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

无

## 调用示例

- 创建int64类型的Const算子：

    ```c++
    EsGraphBuilder builder("test_graph");
    std::vector<int64_t> dims = {3};
    std::vector<int64_t> vec64 = {1, 2, 3};
    auto c1 = builder.CreateConst(vec64, dims);
    ```

- 创建指定类型、格式和维度的Const算子（通用模版方法）：

    ```c++
    EsGraphBuilder builder("test_graph");
    std::vector<float> vecf = {1.1, 2.0, 3.2, 4.4};
    std::vector<int64_t> dims = {4};
    auto c1 = builder.CreateConst<float>(vecf, dims, ge::DT_FLOAT);
    ```
