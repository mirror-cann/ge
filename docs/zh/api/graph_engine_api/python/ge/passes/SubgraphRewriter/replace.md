# replace

## 产品支持情况

全量芯片支持。

## 功能说明

用replacement替换boundary描述的子图。为静态方法。

## 函数原型

```python
@staticmethod
replace(boundary: SubgraphBoundary, replacement: Graph) -> int
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| boundary | 输入 | 子图边界，类型为SubgraphBoundary，描述待替换子图的输入/输出。 |
| replacement | 输入 | 替换图，类型为ge.graph.Graph。 |

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| int | 返回C++ Status的整数形式。失败时可查看日志中关于边界完整性与索引对齐的信息。 |

## 约束说明

replacement必须是非空的图包装器；该图会在C++侧被**拷贝**，但仍需遵循GE对Python Graph所有权的规则。

## 调用示例

```python
from ge.passes import SubgraphBoundary, SubgraphInput, SubgraphOutput, SubgraphRewriter

# n0、out_node为主图中已存在的节点，replacement_graph为已构建好的替换图
b = SubgraphBoundary()
b.add_input(0, SubgraphInput([(n0, 0), (n0, 1)]))
b.add_output(0, SubgraphOutput(out_node, 0))
ret = SubgraphRewriter.replace(b, replacement_graph)
```
