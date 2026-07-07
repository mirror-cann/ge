# Name

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/named\_io\_node\_builder.h\>
- 库文件：libgraph.so、libgraph\_static.a

## 功能说明

设置节点名称（选填）。若不调用此方法，节点名称为空字符串，Build仍可成功。

## 函数原型

```c++
NamedIoNodeBuilder &Name(const char_t *name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 节点名称，用于标识图中该节点。 |

## 返回值说明

返回构建器引用，支持链式调用。

## 约束说明

- 该方法为选填项，未调用Name时节点名称为空，不影响Build。
- 若传入nullptr，Name不会被设置，节点名称为空字符串，Build仍可成功。

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
