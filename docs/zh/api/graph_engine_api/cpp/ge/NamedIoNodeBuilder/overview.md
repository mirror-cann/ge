# 简介

基于输入/输出名称构建图节点的Builder模式类，允许调用方通过指定端口名称而非位置来构造节点。

## 需要包含的头文件

```c++
#include <graph/named_io_node_builder.h>
```

## Public成员函数

```c++
explicit NamedIoNodeBuilder(Graph &graph)
~NamedIoNodeBuilder()
NamedIoNodeBuilder &Type(const char_t *type)
NamedIoNodeBuilder &Name(const char_t *name)
NamedIoNodeBuilder &AddInput(const char_t *name)
NamedIoNodeBuilder &AddInput(const char_t *name, const TensorDesc &desc)
NamedIoNodeBuilder &AddOutput(const char_t *name)
NamedIoNodeBuilder &AddOutput(const char_t *name, const TensorDesc &desc)
NamedIoNodeBuilder &Attr(const char_t *name, const AttrValue &value)
std::unique_ptr<GNode> Build(AscendString &error_message)
```
