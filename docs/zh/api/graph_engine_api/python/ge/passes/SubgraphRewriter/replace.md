# replace

## 产品支持情况

全量芯片支持。

## 功能说明

用replacement替换boundary描述的子图。为静态方法。

## 函数原型

```python
@staticmethod
replace(boundary: SubgraphBoundary, replacement: Graph) -> int

@staticmethod
replace(boundary: SubgraphBoundary, replacement: Graph, *, context: PassContext) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| boundary | 输入 | 子图边界，类型为SubgraphBoundary，描述待替换子图的输入/输出。 |
| replacement | 输入 | 替换图，类型为ge.graph.Graph。 |
| context | 输入 | 可选。当前Pass的PassContext，由GE注入到`FusionBasePass.run(graph, context)`中。传入后，底层自动执行融合可行性检查、子图替换和融合结果上报。 |

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| int | 未传入`context`时，返回C++ Status的整数形式。失败时可查看日志中关于边界完整性与索引对齐的信息。 |
| None | 传入`context`时，成功返回`None`。replacement graph handle失效，或融合检查、子图替换、融合结果上报失败时抛出`RuntimeError`，失败信息会同步到`context`。 |

## 约束说明

- replacement必须是非空的图包装器；该图会在C++侧被**拷贝**，但仍需遵循GE对Python Graph所有权的规则。
- 推荐在`FusionBasePass.run(graph, context)`中传入`context`，由底层完成融合检查和结果上报。
- 不传入`context`时保持原有行为，不自动执行融合结果上报。

## 调用示例

传入`context`和不传入`context`时的逻辑差异如下：

| 调用方式 | 融合可行性检查和结果上报 | 成功返回值 | 失败处理 |
| --- | --- | --- | --- |
| 不传入`context` | 保持原有行为，不自动执行本接口新增的融合可行性检查和融合结果上报 | C++的`Status`整数形式 | 用户检查返回值 |
| 传入`context` | 自动执行融合可行性检查和融合结果上报 | `None` | 抛出`RuntimeError`，并尽量将失败信息同步到`context` |

```python
from ge.passes import SubgraphBoundary, SubgraphInput, SubgraphOutput, SubgraphRewriter

# 以下代码位于FusionBasePass.run(graph, context)中。
# n0、out_node为主图中已存在的节点，replacement_graph为已构建好的替换图。
b = SubgraphBoundary()
b.add_input(0, SubgraphInput([(n0, 0), (n0, 1)]))
b.add_output(0, SubgraphOutput(out_node, 0))
```

完成`boundary`构造后，从以下两种调用方式中选择一种：

### 不传入context

保持原有行为，由用户检查返回值。

```python
ret = SubgraphRewriter.replace(b, replacement_graph)
if ret != 0:
    raise RuntimeError(f"SubgraphRewriter.replace failed, ret={ret}")
```

### 传入context

自动执行融合可行性检查和融合结果上报。成功时返回`None`，失败时抛出`RuntimeError`。

```python
SubgraphRewriter.replace(b, replacement_graph, context=context)
```
