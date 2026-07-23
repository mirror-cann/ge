# 概述

## 什么是AutoFuse

AutoFuse是基于Ascend C的自动融合框架，支持自动融合范围识别、自动算子代码生成、Auto Tiling优化、动态shape及混合精度等特性；在算法网络中，由于存在大量的Vector计算，各个Vector计算之间会产生大量的内存搬运，导致Memory Bound问题。而AutoFuse通过自动将多个算子融合为一个算子，减少网络中的算子数量和内存搬运，从而缓解了Memory Bound问题，释放昇腾算力，提升模型的执行性能。

收益原理示意图如下图所示，自动融合通过将多个算子合并为单个算子，理论上在MTE搬运、动态shape调度开销方面都会有一定的收益，对于小shape、MTE Bound的推荐网络，一般都能获得正收益。

**图 1**  收益原理
![图1示例](../figures/benefit_principle.png "收益原理")

<!-- npu="950,A3,910b" id4 -->
自动融合特性，仅在如下产品型号支持：
<!-- end id4 -->

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id3 -->

## 技术路线

自动融合方案基于昇腾NPU底层统一的Ascend C IR（Ascend C IR是面向Ascend C编程语言做建模的IR，如下简称AscIR）Schedule与代码生成能力，构建了两条融合实现路径，如[图2](#fig1)所示。

- 第一条路径基于昇腾自研的GE框架，注重NPU的亲和性，核心包括三部分能力：
  - Ascend IR（Ascend Intermediate Representation）的符号化shape推导，通过变量符号表达动态变化的shape，从而在编译时基于符号化的shape进行代码生成。
  - Ascend IR到AscIR的Lowering实现，使用低层级的AscIR表达Ascend IR的计算逻辑，确定融合结构。
  - 融合策略，结合AscIR的特点与约束，进行融合结构间的循环轴合并，获得融合最优解。

- 第二条路径则对接PyTorch Inductor，聚焦于生态支持，复用Inductor的融合能力，将Inductor IR表达的融合结构转换为Ascend C IR图，进行代码生成。

**图 2**  技术路线<a id="fig1"></a>
![图2示例](../figures/technical_route.png "技术路线")

## 融合原理

自动融合的实现包含两部分，自动确定融合范围和根据融合范围自动生成融合kernel源码及kernel二进制。前者称为自动融合前端，后者称为自动融合的后端（对应[图2](#fig1)中的公共底层能力）：

前端的实现主要是根据一定规则或配置判断哪些算子能够融合，确定一个融合算子的融合范围，融合范围用FusedGraph来表达，如下图[图3](#fig2)所示，FusedGraph内部包含\>=1个AscBackend节点，AscBackend可以理解为一个类似于ge::op::partitionedcall的Ascend IR算子，其携带一个子图对象，一个AscBackend节点携带一个AscGraph属性，一个AscGraph内包含多个AscIR节点。AscIR与AscGraph详细介绍请参见[AscIR与AscGraph](../appendix/ascir_and_ascgraph.md)。

**图 3**  FusedGraph<a id="fig2"></a>
![图3示例](../figures/fused_graph.png "FusedGraph")

**图 4**  AscBackend对应的AscGraph
![图4示例](../figures/ascbackend_ascgraph.png "AscBackend对应的AscGraph")

后端的实现包含Schedule/Codegen/Auto Tiling等，后端接收到FusedGraph后，Schedule首先针对计算/搬运类节点分别生成对应的TilingGroup，并通过TilingGroup合并策略，构建适用于整个融合算子的归一化TilingGroup。最终基于该归一化TilingGroup来生成FusedGraph的Tiling策略。经过Codegen实现和Auto Tiling，生成kernel源码、tiling\_func源码、tiling\_data源码、并编译生成Host和Device交付件。 自动融合的生效方式请参见[AutoFuse开启方式](autofuse_enable.md)。[Schedule](Schedule.md)/[Codegen](Codegen.md)/[Auto Tiling](Auto-Tiling.md)的实现原理请分别参见对应章节。
