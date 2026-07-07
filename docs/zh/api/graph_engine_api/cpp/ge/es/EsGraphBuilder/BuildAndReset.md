# BuildAndReset

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

构建计算图。

## 函数原型

- 构建计算图

```c++
    std::unique_ptr<Graph> BuildAndReset() const
    ```

-   根据输出张量列表构建计算图

    ```
    std::unique_ptr<Graph> BuildAndReset(const std::vector<EsTensorHolder> &outputs)
    ```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| outputs | 输入 | 输出张量持有者向量。 |

## 返回值说明


| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::unique_ptr<Graph> | 返回构建的图智能指针，失败时返回nullptr。调用后图构建器将不再可用。 |

## 约束说明

无

## 调用示例

-   构建计算图

```c++
    // 1. 创建图构建器（EsGraphBuilder）
    EsGraphBuilder builder("graph_name");
    // 2. 添加 2 个输入节点
    EsTensorHolder [data0, data1] = builder.CreateInputs<2>();
    // 3. 添加中间节点，C++中，加减乘除等常用运算符被重载，可以直接使用
    EsTensorHolder add = data0 + data1;
    // 4. 设置图输出
    builder.SetOutput(add, 0);
    // 5. 完成构图，获取构造好的`Graph`对象，`builder`中的资源随析构而销毁
    std::unique_ptr<ge::Graph> graph = builder.BuildAndReset();
    ```

-   根据输出张量列表构建计算图

    ```
    // 1. 创建图构建器（EsGraphBuilder）
    EsGraphBuilder builder("graph_name");
    // 2. 添加 2 个输入节点
    EsTensorHolder [data0, data1] = builder.CreateInputs<2>();
    // 3. 添加中间节点，C++中，加减乘除等常用运算符被重载，可以直接使用
    // 4. 设置图输出
    // 5. 完成构图，获取构造好的`Graph`对象，`builder`中的资源随析构而销毁
    std::unique_ptr<ge::Graph> graph = builder.BuildAndReset({data0 + data1}); // 一行中完成3，4，5
    ```
