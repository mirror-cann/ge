# CreateInputs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建指定数量的输入节点。

## 函数原型

- 创建指定数量的输入节点，返回数组，适用于编译时的结构化绑定

```c++
    template <size_t inputs_num>
    std::array<EsTensorHolder, inputs_num> CreateInputs(size_t start_index = 0)
    ```

-   创建指定数量的输入节点，返回容器，适用于运行时动态绑定

    ```
    std::vector<EsTensorHolder> CreateInputs(size_t inputs_num, size_t start_index = 0)
    ```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| inputs_num | 输入 | 输入节点数量。 |
| start_index | 输入 | 输入节点起始索引，如果不为0表示前面已经创建了其他输入节点，整体输入节点的索引应该从0开始连续递增。 |

## 返回值说明


| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::array<EsTensorHolder, inputs_num> | 返回创建的输入张量持有者数组。 |
| - | std::vector<EsTensorHolder> | 返回创建的输入张量持有者容器。 |

## 约束说明

无

## 调用示例

-   创建指定数量的输入节点，返回数组，适用于编译时的结构化绑定

```c++
    // 1. 创建图构建器（EsGraphBuilder）
    EsGraphBuilder builder("graph_name");
    // 2. 添加 2 个输入节点，使用c++17的结构化绑定到data0和data1
    EsTensorHolder [data0, data1] = builder.CreateInputs<2>();
    ```

-   创建指定数量的输入节点，返回容器，适用于运行时动态绑定

    ```
    // 1. 创建图构建器（EsGraphBuilder）
    EsGraphBuilder builder("graph_name");
    // 2. 添加 2 个输入节点
    std::vector<EsTensorHolder> inputs = builder.CreateInputs(2);
    ```
