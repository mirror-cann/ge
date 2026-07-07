# Type

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/named\_io\_node\_builder.h\>
- 库文件：libgraph.so、libgraph\_static.a

## 功能说明

设置节点类型（必填）。节点类型对应的算子IR需已通过REG\_OP完成注册，否则Build时返回nullptr。

## 函数原型

```c++
NamedIoNodeBuilder &Type(const char_t *type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| type | 输入 | 算子类型，如"Add"、"MatMul"等，对应算子IR需已通过REG_OP完成注册。 |

## 返回值说明

返回构建器引用，支持链式调用。

## 约束说明

- 该方法为必填项，未调用Type时Build将返回nullptr。
- 若传入nullptr，Type不会被设置，Build时将报错"Type must be set before Build\(\)"。

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
