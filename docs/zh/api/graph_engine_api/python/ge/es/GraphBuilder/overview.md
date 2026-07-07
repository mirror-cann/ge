# 简介

GraphBuilder类用于以Eager Style方式构建计算图。

Eager Style图构建调用示例：

```python
from ge.es import GraphBuilder

# 创建构建器
builder = GraphBuilder("my_graph")

# 创建输入
input1 = builder.create_input(0, shape=[2, 2])
input2 = builder.create_input(1, shape=[2, 2])

# 创建常量
const1 = builder.create_const_float(1.0)

# 张量运算
result = input1 + input2

# 设置输出
builder.set_graph_output(result, 0)

# 构建图
graph = builder.build_and_reset()
```
