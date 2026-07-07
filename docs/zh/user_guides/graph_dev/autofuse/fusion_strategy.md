# 融合策略

## 简介

融合策略需要综合考虑后端能力、性能收益、内存峰值影响对融合行为做出决策和融合处理，得到一个相对较优的融合结果。融合策略当前实现的功能如下：

- 支持统一的融合规则判断。
- 支持动静态shape（依赖[符号化](symbolization.md)）。
- 支持融合规模控制。
- 支持融合优先级处理。

融合策略模块由CanFuse框架和Backend两部分组成：

- **CanFuse框架**：支持融合判断（成环检测，内存峰值等），融合策略求解功能。
- **Backend**：支持融合规则，融合处理（循环合并，AscGraph融合、AscBackend融合）功能。

两部分交互主流程如下：

**图 1**  CanFuse与Backend交互流程图
![图1](../figures/canfuse_backend_interaction_flow.png "CanFuse与Backend交互流程图")

由[Lowering](Lowering.md)模块做的融合为一次融合，由CanFuse框架做的融合为二次融合，二次融合简要流程如下：

1. 首先进行一轮融合，默认做10轮。
2. 获取可能融合的所有节点对。
3. 从上步得到的可融合节点对中获取融合优先级。
4. 根据优先级，融合策略的分值，进行排序。
5. 调用CanFuse框架进行融合规则判断：
    1. 确定是否能进行垂直融合。

        垂直融合：两个融合节点之间有输入输出关系，一个节点的输出为另一个节点的输入。

    2. 进行内存峰值影响分析。
    3. 确定是否能进行水平融合。

        水平融合：两个节点间没有输入输出关系，但是有来自同一个根节点的输入。

6. 对两个节点进行融合。

## 融合条件判断

判断两个节点是否能融合，需要综合CanFuse框架和Backend的判断结果，两个结果都必须能融合才能融合，本章节分别给出两者的判断条件。

### CanFuse框架判断融合条件

- 能够减少内存读写

    可融合的节点间必须有共用的内存，比如下图所示，Node1的输出1是Node3的输入可以融合，Node3和Node4输入都来自Node1的输出1可以融合，融合后都可以节省内存搬运。Node2和Node3虽然输入都来自Node1，但不是Node1的同一个输出，融合后还是要搬入两次，不能减少内存搬运，因此不能融合。

    **图 1**  CanFuse判断融合条件1
    ![图1](../figures/canfuse_fusion_condition_1.png "CanFuse判断融合条件1")

- 不会导致成环

    如下图所示：Node1和Node3融合后会导致成环，因此不能融合。

    **图 2**  CanFuse判断融合条件2
    ![图2](../figures/canfuse_fusion_condition_2.png "CanFuse判断融合条件2")

- 不超过最大融合个数限制（默认最大融合个数为64）

    融合规模控制主要是防止后端资源超限，节点数按照Lowering生成的AscGraph里的节点数进行控制，比如下图中的Node1和Node2融合后认为节点总数是9，可以融合；如果两个节点融合后总数超出阈值就不能融合。

    **图 3**  CanFuse判断融合条件3
    ![图3](../figures/canfuse_fusion_condition_3.png "CanFuse判断融合条件3")

- 不会导致内存峰值增加

    过度的融合可能会导致内存峰值的增加，需要在执行性能和内存之间进行平衡，完备的内存峰值评估是一个比较复杂的过程，先按照简单策略进行评估，对于节点跨度（拓扑排序的ID差值）超过设定阈值的不再进行融合，当节点可以水平融合的时候，会先计算融合后节点的输出内存是否超过13G，如果超过13G则不融合。

    比如下图所示：Node2-NodeN之间在融合前是有机会进行内存复用的，融合后会导致内存峰值增加，因此不能融合。

    **图 4**  CanFuse判断融合条件4
    ![图4](../figures/canfuse_fusion_condition_4.png "CanFuse判断融合条件4")

### Backend判断融合条件

- 两个AscGraph的loop轴要能映射

    能够融合的第一个条件是两个AscBackend的loop轴是否可以映射，如果不是一套loop轴，则判断融合是没有意义的。由于每个AscBackend是独立进行Lowering的，各自的循环轴ID也是独立编号，因此可能会存在轴ID不一致的问题，典型场景如下图所示：AscBackend1做了reduce后少了一个循环轴，AscBackend2中z1等同AscBackend1中的z2，AscBackend2中z2等同AscBackend1中的z3，如果要融合，则需要把AscBackend2中的loop轴调整为跟AscBackend1相同。

    **图 5**  Backend判断融合条件
    ![图5](../figures/backend_fusion_condition.png "Backend判断融合条件")

- 两个AscGraph要能满足Schedule的group merge规则

    假设对轴做一个抽象分组，分为三个group：xgroup、ygroup、rgroup，其中：

  - **xgroup:**  为Concat类算子引入的一个单独group，Concat轴之前的轴是xgroup，之后的是ygroup。
  - **ygroup：**Elementwise、Broadcast类型的算子循环轴。
  - **rgroup：**Reduce轴的集合。

    每个AscGraph都会有一个基于循环轴的\(xgroup, ygroup, rgroup\)，根据算子融合规则推导，可以判断两个AscGraph是否能融合成一个新的group；然后CanFuse依据此规则，判断后端Schedule是否支持融合。详细group merge规则请参见[生成TilingCase](Schedule.md#生成tilingcase)。

## 融合策略求解

经过[融合条件判断](#融合条件判断)后，若发现两个节点可以融合，可能会产生多种融合结果的组合，例如下图中的融合策略1和融合策略2，当存在多种融合策略时，需要确定哪种策略为最优。

**图 1**  多种融合策略场景
![图1](../figures/multi_fusion_strategy_scenario.png "多种融合策略场景")

计算整图的融合策略最优解是较为困难的，主要原因有以下两点：

1. 算法复杂度可能较高，一种可行的思路是动态规划求解，保证在nlog\(n\)有难度。
2. 性能估算模型不准确，没有上板实际测试前，难以准确确定融合后的真实性能。

基于上述原因，追求严格的全局最优解必然涉及大量的整图融合尝试和上板实测，从而导致时间复杂度显著增加。因此，现阶段采用**score+贪心的简单融合策略求解算法**：将图上可融合节点两两配对，逐个进行CanFuse判断，在可融合的情况下，再根据打分进行排序（以决定融合的先后顺序），最后依次进行融合处理。第一轮融合后的节点将再次重复该处理，尝试进行多轮融合，最多融合10轮。融合打分规则：

1. 节省的内存大小计算（利用符号实现大小比较，越大优先级越高）。
2. 节点间的临近性计算（使用topo序ID的差值，越小优先级越高）。

排序优先级：

首先比较内存大小，当内存大小相同时，再比较临近性；若临近性也相同，则按topo序从小到大排列，并进行去重处理，最终得到可融合的节点对，后续将按此顺序依次进行融合处理。

以[图2](#fig1)、[图3](#fig2)为例简单说明贪心算法的使用：

如[图2](#fig1)所示，A、B、C、D组成AB、BC、CD融合节点对，排序后确定融合顺序，假设融合顺序CD优先，AB次之，融合后变成CD、AB；当处理BC融合时B已经变成AB，C已经变成CD，最后变成AB+CD的融合处理。

**图 2**  融合例子1<a id="fig1"></a>
![图2](../figures/fusion_example_1.png "融合例子1")

如[图3](#fig2)所示，如果排序是AB优先，BC次之融合过程会发生变化，AB先融合，在处理BC融合时由于B已经变成了AB会进行AB+C的融合，融合成ABC，同理CD融合会进行ABC+D的融合。

**图 3**  融合例子2<a id="fig2"></a>
![图2](../figures/fusion_example_2.png "融合例子2")

两个场景的最终融合结果相同，都是ABCD。然而，假设A和D不能融合，实际结果会有所不同：[图2](#fig1)的结果是AB、CD，而[图3](#fig2)的结果是ABC、D。由此可见，融合的顺序变化会导致融合过程和结果的不同。当前算法也存在局限性：当前的贪心算法仅考虑局部最优，可能会导致全局并非最优。例如，当A和D不能融合时，如果优先融合CD，可能会导致ABC无法融合，从全局角度来看，可能并非是最优策略。

## 融合节点处理

AscBackend和AscBackend之间才可以进行融合，融合后，可以循环轴合并生成的仍然是AscBackend类型，无法循环轴合并的，则生成FusedAscBackend类型。

节点融合需要融合原图中的AscBackend，并同时对AscBackend下的AscGraph进行子图融合，子图融合过程中需要消除AscGraph上的load和store节点。融合节点处理原则如下，示意图如[图1](#fig1)所示。

- 根据循环轴是否相同，判断两个AscBackend是否能循环轴合并，如果能循环轴合并，合并AscBackend下的两个AscGraph。（具体合并流程可参考[图2](#fig2)）。

- 如果不能循环轴合并，则将AscBackend放入一个新FusedAscBackend的子图中，由后端决定生成多个kernel还是一个kernel包含多个循环。
- 不能循环合并的FusedAscBackend与原图中的AscBackend直接融合成一个新FusedAscBackend。
- FusedAscBackend的子图中可能还存在可以循环合并的AscBackend，框架应尽可能地进行循环轴合并。

**图 1**  融合节点处理<a id="fig1"></a>
![图1](../figures/fusion_node_processing.png "融合节点处理")

**图 2**  循环轴合并示意图<a id="fig2"></a>
![图2](../figures/loop_axis_merge_diagram.png "循环轴合并示意图")
