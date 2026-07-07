# 简介

offline\_compile模块用于离线图编译。

离线图编译操作调用示例：

```python
from ge.graph import Graph
from ge.offline_compile import build_initialize, build_finalize, build_model, save_model

# 创建Graph
graph = Graph("test_graph")
# 初始化模型构建
build_initialize({"ge.socVersion": "Ascend910B1"})
# 编译模型
model = build_model(graph, {"input_format": "ND"})
# 保存模型
save_model("sample", model)
# 释放模型构建资源
build_finalize()
```
