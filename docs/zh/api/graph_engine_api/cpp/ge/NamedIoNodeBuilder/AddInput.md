# AddInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/named\_io\_node\_builder.h\>
- 库文件：libgraph.so、libgraph\_static.a

## 功能说明

添加输入实例端口。用户按IR定义顺序依次添加输入实例，普通/可选输入使用IR名，动态输入使用ir\_name0、ir\_name1、……形式的连续实例名。

## 函数原型

```c++
NamedIoNodeBuilder &AddInput(const char_t *name)
NamedIoNodeBuilder &AddInput(const char_t *name, const TensorDesc &desc)
```

- 第一个原型：添加输入实例端口，使用默认TensorDesc（DT\_FLOAT + FORMAT\_ND）。
- 第二个原型：添加带描述的输入实例端口，可指定数据类型、格式和形状。

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 输入实例名称。普通/可选输入使用IR名，动态输入使用ir_name0、ir_name1、……形式的连续实例名。 |
| desc | 输入 | 输入张量描述，包含数据类型、格式和形状信息。仅在第二个原型中使用。 |

## 返回值说明

返回构建器引用，支持链式调用。

## 约束说明

- 输入实例需按IR定义顺序添加。
- 可选输入（OPTIONAL\_INPUT）的使用方式：
    - 不传递可选输入：仅添加必选输入，跳过该可选输入即可。
    - 传递有效可选输入：按IR顺序在对应位置添加，使用默认TensorDesc。
    - 传递占位可选输入：使用带TensorDesc的重载，传入DT\_UNDEFINED和FORMAT\_RESERVED。
    - 可选输入最多只能提供一个实例，重复添加会导致校验失败。

- 若传递动态输入实例，实例名需从ir\_name0开始连续编号，不能跳过中间序号。
- 若传入nullptr为name，该输入不会被添加。

## 调用示例

```c++
ge::Graph graph("test_graph");
ge::AscendString error_msg;

// 构建一个Add算子节点
auto node = ge::NamedIoNodeBuilder(graph)
    .Type("Add")
    .Name("add_node")
    .AddInput("x1")
    .AddInput("x2")
    .AddOutput("y")
    .Build(error_msg);

if (node != nullptr) {
    // 构建成功，节点已被添加到graph中
} else {
    // 构建失败，查看error_msg获取错误信息
}
```
