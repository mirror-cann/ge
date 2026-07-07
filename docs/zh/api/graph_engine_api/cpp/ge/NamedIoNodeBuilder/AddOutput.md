# AddOutput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/named\_io\_node\_builder.h\>
- 库文件：libgraph.so、libgraph\_static.a

## 功能说明

添加输出实例端口。用户按IR定义顺序依次添加输出实例，普通输出使用IR名，动态输出使用ir\_name0、ir\_name1、……形式的连续实例名。

## 函数原型

```c++
NamedIoNodeBuilder &AddOutput(const char_t *name)
NamedIoNodeBuilder &AddOutput(const char_t *name, const TensorDesc &desc)
```

- 第一个原型：添加输出实例端口，使用默认TensorDesc（DT\_FLOAT + FORMAT\_ND）。
- 第二个原型：添加带描述的输出实例端口，可指定数据类型、格式和形状

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 输出实例名称。普通输出使用IR名，动态输出使用ir_name0、ir_name1、……形式的连续实例名。 |
| desc | 输入 | 输出张量描述，包含数据类型、格式和形状信息。仅在第二个原型中使用。 |

## 返回值说明

返回构建器引用，支持链式调用。

## 约束说明

- 输出实例需按IR定义顺序添加。
- 若传递动态输出实例，实例名需从ir\_name0开始连续编号，不能跳过中间序号。
- 必选输出必须添加，否则Build返回nullptr。
- 若传入nullptr为name，该输出不会被添加。

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
