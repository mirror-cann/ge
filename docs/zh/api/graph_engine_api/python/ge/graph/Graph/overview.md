# 简介

Graph类用于表示和操作计算图。

基本图操作调用示例：

```python
from ge.graph import Graph, DataType, Format

# 创建图
graph = Graph("my_graph")

# 获取所有节点
nodes = graph.get_all_nodes()

# 设置和获取属性
graph.set_attr("my_attr", "value")
attr_value = graph.get_attr("my_attr")

# 导出图
graph.dump_to_file(format=DumpFormat.kReadable, suffix="debug")
```
