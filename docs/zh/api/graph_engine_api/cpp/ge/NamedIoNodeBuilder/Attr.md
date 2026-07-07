# Attr

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/named\_io\_node\_builder.h\>
- 库文件：libgraph.so、libgraph\_static.a

## 功能说明

设置节点属性。Build时用户设置的属性优先，已注册IR定义中的默认属性仅用于补全，不覆盖用户设置值。

## 函数原型

```c++
NamedIoNodeBuilder &Attr(const char_t *name, const AttrValue &value)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 属性名称。 |
| value | 输入 | 属性值，通过AttrValue::SetAttrValue构造。 |

## 返回值说明

返回构建器引用，支持链式调用。

## 约束说明

- 若传入nullptr为name，该属性不会被设置。
- 若AttrValue未设置值（默认构造），Build时可能返回nullptr。
- 用户设置的属性值不会被已注册IR定义中的默认属性覆盖。

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
