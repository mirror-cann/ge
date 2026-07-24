# report\_fuse

## 产品支持情况

全量芯片支持。

## 功能说明

GE融合Pass支持以下两种改图方式：

- 传统直接改图方式：用户在[FusionBasePass.run(graph, context)](FusionBasePass/run.md)中直接调用Graph接口修改原图，例如增删边、删除节点或重连节点。由于改图过程由用户控制，框架无法自动确定融合前后的节点集合。用户需要在改图前调用`can_fuse`执行融合可行性检查，完成自定义改图后、删除旧节点前调用本函数上报融合结果。
- 基于子图替换的方式：这是后续提供的高效改图方式。用户通过boundary描述待替换子图的边界，通过replacement描述替换图，再调用[SubgraphRewriter.replace](SubgraphRewriter/replace.md)完成改图。传入`context`时，该接口会自动完成融合可行性检查、子图替换和融合结果上报，无需单独调用`can_fuse`和本函数。

本函数用于传统直接改图方式。函数根据用户传入的融合前、融合后节点集合更新融合维测信息，并记录当前Pass名称。

## 函数原型

```python
report_fuse(
    nodes_before: Iterable[Node],
    nodes_after: Iterable[Node],
    context: PassContext,
) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| nodes_before | 输入 | 融合前的连通节点集合，元素必须为ge.graph.Node对象。 |
| nodes_after | 输入 | 融合后的连通节点集合，元素必须为ge.graph.Node对象；空集合表示只删除旧节点、不新增节点。 |
| context | 输入 | 当前Pass的PassContext，由GE注入到`FusionBasePass.run(graph, context)`中。 |

## 返回值说明

成功时返回`None`。节点集合中包含非ge.graph.Node对象，或`context`不是PassContext时抛出`TypeError`；Node handle失效或底层融合结果上报失败时抛出`RuntimeError`，失败信息会同步到`context`。

## 约束说明

- 必须先完成自定义改图，再调用本函数。
- 必须在旧节点被删除或失效前调用本函数，确保`nodes_before`仍然有效。
- `nodes_before`中的节点必须属于同一张图；非空的`nodes_after`也必须属于该图。
- `PassContext`仅在当前Pass同步调用期间有效，不应保存到实例属性或其他线程中使用。

## 调用示例

以下示例采用传统直接改图方式，删除一个仅有一个数据输入和一个数据输出的中间节点，并将其上游节点直接连接到所有下游节点。

```python
from ge.passes import can_fuse, report_fuse

def remove_intermediate_node(graph, node, context):
    nodes_before = [node]
    result = can_fuse(nodes_before)
    if not result.ok:
        context.set_error_message(result.reason)
        return False

    input_node, input_output_index = node.get_in_data_nodes_and_port_indexes(0)
    consumers = list(node.get_out_data_nodes_and_port_indexes(0))

    graph.remove_edge(input_node, input_output_index, node, 0)
    for consumer, consumer_input_index in consumers:
        graph.remove_edge(node, 0, consumer, consumer_input_index)
        graph.add_data_edge(
            input_node,
            input_output_index,
            consumer,
            consumer_input_index,
        )

    # 本次改图未新增节点，因此nodes_after传入空列表。
    # 上报完成前，nodes_before中的节点必须保持有效。
    report_fuse(nodes_before, [], context)
    graph.remove_node(node)
    return True
```
