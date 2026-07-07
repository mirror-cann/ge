# --topo\_sorting\_mode

## 产品支持情况

全量芯片支持

## 功能说明

对算子进行图模式编译时，可选择的不同图遍历模式。

## 关联参数

无

## 参数取值

- 0：BFS，Breadth First Search，广度优先遍历策略。搜索算法的一种，与DFS类似，从某个状态出发，搜索所有可以到达的状态；与DFS的区别是，BFS是一层层进行遍历。
- 1：（默认值）DFS，Depth First Search，深度优先遍历策略。搜索算法的一种，从某个状态开始，不断转移状态直到无法转移，然后回退到前一步的状态，继续转移到其他状态，按此重复，直到找到最终解。
- 2：RDFS，Reverse DFS，反向深度优先遍历策略。
- 3：StableRDFS，稳定拓扑序策略，针对图里已有的算子，不会改变其计算顺序；针对图里新增的算子，使用RDFS遍历策略。

## 推荐配置及收益

无。

## 示例

```bash
atc --topo_sorting_mode=3 ...
```

## 使用约束

模型转换过程中如果加载了自定义知识库路径，上述图遍历模式的改变可能会影响调优结果，建议重新进行调优，详情请参见[《AOE调优工具》](https://hiascend.com/document/redirect/CannCommunityToolAoe)。
