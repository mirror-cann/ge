# NamedIoNodeBuilder构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/named\_io\_node\_builder.h\>
- 库文件：libgraph.so、libgraph\_static.a

## 功能说明

NamedIoNodeBuilder构造函数，创建基于输入、输出和属性名称构造图节点的Builder对象。

NamedIoNodeBuilder面向直接按照节点输入/输出名称构图的场景。调用方可指定节点实例的输入名、输出名、属性名及相关描述信息；构建时根据节点类型匹配已通过REG_\__OP注册的算子IR定义，校验输入/输出实例名称及顺序是否与该定义兼容，最终创建对应的图节点。

## 函数原型

```c++
explicit NamedIoNodeBuilder(Graph &graph)
~NamedIoNodeBuilder()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 所属的图对象，构建完成的节点将被添加到此图中。 |

## 返回值说明

构造函数返回NamedIoNodeBuilder类型的对象。

## 异常处理

无

## 约束说明

- NamedIoNodeBuilder对象不可拷贝构造和赋值。
- 构建器对象Build成功后不应再次使用。
