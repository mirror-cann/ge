# Build

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/named\_io\_node\_builder.h\>
- 库文件：libgraph.so、libgraph\_static.a

## 功能说明

构建图节点并添加到Graph。Build会基于已设置的输入、输出和属性信息创建GNode，并校验输入/输出实例名称及顺序是否与已注册的算子IR定义兼容。成功时返回的GNode已被添加到Graph中。

Build执行以下对外可见校验和操作：

1. 校验Type已设置，且节点类型对应的算子IR已通过REG\_OP完成注册。
2. 校验输入/输出实例名称及顺序是否与已注册的算子IR定义兼容。
3. 创建GNode并添加到Graph。
4. 返回unique\_ptr<GNode\>，成功时清空error\_message。

## 函数原型

```c++
std::unique_ptr<GNode> Build(AscendString &error_message)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| error_message | 输出 | 构建失败时的错误信息。成功时error_message被清空。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::unique_ptr\<GNode> | 成功时返回GNode的unique_ptr，已被添加到Graph中。失败时返回nullptr。 |

## 约束说明

- Builder对象Build成功后不应再次使用，Build成功后再次调用返回nullptr并输出错误信息"Build\(\) has already been called on this builder"。
- 节点类型对应的算子IR需已通过REG\_OP完成注册，否则Build返回nullptr。
- 输入/输出实例需按IR定义顺序添加，否则Build返回nullptr。
- Data和NetOutput节点不进行输入/输出实例兼容性校验。

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
