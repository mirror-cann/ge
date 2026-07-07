# CreateInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建输入节点。

## 函数原型

- 创建图输入节点

    ```c++
    EsTensorHolder CreateInput(int64_t index,const char *name,const char *type)
    ```

- 创建默认输入节点，从0开始计数，节点命名为input\_\{index\}

    ```c++
    EsTensorHolder CreateInput(int64_t index)
    ```

- 创建指定名称的输入节点

    ```c++
    EsTensorHolder CreateInput(int64_t index, const char *name)
    ```

- 创建具有完整信息的输入节点

    ```c++
    EsTensorHolder CreateInput(int64_t index, const char *name, ge::DataType data_type, ge::Format format, const std::vector<int64_t> &shape)
    ```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 输入节点的索引，从0开始计数。 |
| name | 输入 | 输入节点的名称，如果为nullptr则使用默认名称input_{index}。 |
| type | 输入 | 输入节点的类型字符串，如果为nullptr则默认为Data。 |
| format | 输入 | 张量格式。 |
| data_type | 输入 | 数据类型。 |
| shape | 输入 | 张量形状向量，如果为空则创建标量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsTensorHolder | 返回创建的输入张量持有者，失败时返回无效的EsTensorHolder。 |

## 约束说明

无

## 调用示例

- 创建图输入节点

```c++
    EsGraphBuilder builder("graph_name");
    auto t1 = builder.CreateInput(0, "input0", "Data");
    ```

-   创建默认输入节点，从0开始计数，节点命名为input\_\{index\}

    ```c++
    EsGraphBuilder builder("graph_name");
    auto tensor = builder.CreateInput(1);
    ```

-   创建指定名称的输入节点

    ```c++
    EsGraphBuilder builder("graph_name");
    auto tensor = builder.CreateInput(2, "input2");
    ```

-   创建具有完整信息的输入节点

    ```c++
    EsGraphBuilder builder("graph_name");
    auto tensor = builder.CreateInput(3, "input3", ge::DT_INT32, ge::FORMAT_NCHW, {2, 2});
    ```
