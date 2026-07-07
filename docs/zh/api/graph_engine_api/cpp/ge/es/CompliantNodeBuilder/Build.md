# Build

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

构建并返回图节点。

## 函数原型

```c++
ge::GNode Build() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | GNode | 构建完成的图节点对象。 |

## 约束说明

无

## 调用示例

```c++
auto ge_graph = std::make_unique<ge::Graph>("graph");
auto add_node = ge::es::CompliantNodeBuilder(ge_graph.get()).OpType("Add")
      .Name("add_0")
      .IrDefInputsV2({
          {"x1", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""},
          {"x2", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""},
      })
      .IrDefOutputsV2({
          {"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""},
      })
      .IrDefAttrsV2({
      })
      .Build();
// 示例中通过链式调用根据Add的ir原型，在图中创建了一个Add算子的实例add_0

```
