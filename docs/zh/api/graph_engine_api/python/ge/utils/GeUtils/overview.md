# 简介

GE改图相关的工具类。如shape推导，校验节点是否支持，部分图优化工具方法等。
调用示例如下：

```python
from ge.utils import GeUtils
from ge.graph import Graph, Node

# Shape 推导
graph = Graph("my_graph")
# ... 构建图 ...
GeUtils.infer_shape(graph, [[1, 3, 224, 224], [1, 3, 224, 224]])

# 检查节点是否在 AICore 上支持
node = graph.get_node("node_name")
is_supported, reason = GeUtils.check_node_support_on_aicore(node)
if is_supported:
    print("节点在 AICore 上支持")
else:
    print(f"节点在 AICore 上不支持，原因：{reason}")
