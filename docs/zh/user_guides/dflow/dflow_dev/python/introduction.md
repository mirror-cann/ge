# 概述

## DataFlow简介

DataFlow是一套面向异构系统的应用开发和执行框架，提供了一套API来帮助用户快速表达业务逻辑。框架屏蔽异构硬件和组网的差异，并以异步流水方式提升应用的整体性能和吞吐量。同时，DataFlow提供了简单灵活的部署策略，用户可以方便地调整每个业务实例的个数以及部署位置，从而提升应用的资源利用率，降低用户的使用成本。

下图是一个应用DataFlow使能异步流水提升吞吐的示例：

![](figures/260109164513685.png)

示例应用由3个计算功能组成，其中计算功能1和计算功能3为用户自定义函数，计算功能2为计算图执行，原始流程中3个计算流程串行执行。

DataFlow通过FlowGraph来承载整个计算流，每个计算功能对应图上的一个FlowNode节点，并通过FuncProcessPoint或者GraphProcessPoint来承载节点功能。节点间通过异步数据流连接，当FlowGraph有多组数据输入时，多个FlowNode能够同时处理不同的数据，即将原始的串行执行流程转换为了异步流水的执行过程。

DataFlow各个节点的关系如下所示：

![](figures/260109164713036.png)

- FlowGraph：DataFlow的Graph，由输入节点FlowData和计算节点FlowNode构成，是DAG（Directed Acyclic Graph）图，数据流有向且不允许有成环表达。
- FlowData：FlowGraph的输入节点。FlowData定义了输入节点的名称和类型。
- FlowNode：FlowGraph的计算节点。FlowNode定义了计算节点的名称、输入个数、输出个数。支持如下两种类型。
  - FuncProcessPoint：Func的计算处理点，通过UDF（User Defined Function）实现用户自定义功能。
  - GraphProcessPoint：Graph的计算处理点，通过IR构图实现用户的计算逻辑。

DataFlow支持用户通过FuncProcessPoint和GraphProcessPoint编写自定义处理函数，通过DataFlow构图以FlowModel的方式运行。

## DataFlow应用场景

### 多模型串接下沉执行

DataFlow可以对自定义计算节点或模型进行编排，通过数据驱动模型执行，其中被定义成GraphProcessPoint的节点可以完全下沉device侧执行，且相邻的两个GraphProcessPoint之间的数据传输将在device-device间进行。上述机制减少了host和device之间控制面和数据面的交互，降低整个DataFlow图执行的时延，从而提升整体性能。

如下图所示DataFlow图中包含了两个FuncProcessPoint（UDF0和UDF1），两个GraphProcessPoint分别用于加载onnx模型和pb模型进行推理，GraphProcessPoint可以完全下沉到device执行，数据在UDF0、UDF1处理结束后传递到device，两个模型的执行和数据传递将完全在device进行。

![](figures/260109165007544.png)

### 基于PyTorch模型的在线推理

DataFlow可以结合多种深度学习框架进行模型的训练或在线推理，来达到提高模型吞吐的目的。以PyTorch为例，在使用PyTorch进行模型在线推理时，通常会经过预处理、模型推理以及后处理三个步骤，其中模型推理又可以根据模型结构拆分成多个串行执行的子模型。

根据上述业务逻辑，DataFlow能够将预处理、多个子模型推理以及后处理封装成能够异步执行的多个FuncProcessPoint，并通过异步队列将它们间的数据流串接起来，如下图所示：

![](figures/260109165112546.png)

通过将原始串行执行流程转换为一张FlowGraph图，在进行多轮推理时，能够使能不同模块间的异步流水，从而提升应用的整体吞吐量。
