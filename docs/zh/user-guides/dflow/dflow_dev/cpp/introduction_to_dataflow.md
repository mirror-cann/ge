# DataFlow简介

## 读者对象

本文档用于指导开发者如何使用DataFlow接口进行计算图的构建、修改、编译和执行。通过本文档您可以达成以下目标：

- 了解通过FlowOperator构建FlowGraph的方法。
- 掌握编译运行FlowGraph的方法。
- 了解UDF开发过程及相关接口。

熟悉CANN软件基本架构以及特性、具备C++/C语言程序开发能力，对机器学习、深度学习有一定了解的人员，可以更好地理解本文档。

## DataFlow概述

DataFlow用于描述采用数据队列以数据驱动方式将一个或多个计算处理点（ProcessPoint）组织成完整的计算流。ProcessPoint与算子的关键区别是采用异步方式处理。DataFlow通过FlowGraph来承载，ProcessPoint通过FlowNode来承载，各个接口之间的关系如下所示。

**图1**  DataFlow相关接口之间的关系  
![](figures/DataFlow相关接口之间的关系.png "DataFlow相关接口之间的关系")

- FlowGraph：DataFlow的Graph，由输入节点FlowData和计算节点FlowNode构成。
- FlowOperator：是FlowGraph的节点基类，衍生类有FlowData和FlowNode两种类型。
- FlowData：FlowGraph的输入节点。
- FlowNode：FlowGraph的计算节点。支持如下两种类型。
  - FunctionPp：Function的计算处理点，通过UDF（User Defined Function）实现用户自定义功能。
  - GraphPp：Graph的计算处理点，通过IR构图实现用户的计算逻辑。

DataFlow支持用户通过FunctionPp和GraphPp编写自定义处理函数，通过DataFlow构图以FlowModel的方式运行。

DataFlow和IR构图的不同点如下所示。

**表1**  DataFlow和IR构图比较

|维度|IR构图|DataFlow|
|--|--|--|
|数据流处理方式|- 图只支持一次输入对应一次输出。<br>- 图中的算子之间采用同步数据流，用于表达串行，同步执行。|- DataFlow支持一次输入对应多次输出，或者一次输入对应一次输出，或者多次输入对应一次输出。灵活性更高。<br>-DataFlow中的ProcessPoint采用异步数据流，可以表达并行，异步执行，充分利用资源，提升吞吐量。|
|自定义功能开发方式|通过开发自定义算子实现。<br>算子包括算子原型定义、算子代码实现、算子信息库定义、算子适配等过程，需要用户开发的交付件多，使用门槛相对较高。|可以通过开发UDF实现，也可以通过开发自定义算子实现。<br>UDF开发只需要定义用户函数，构建图。用户交付件少，使用门槛低。|
|内存分配方式|算子的输入内存、输出内存是已经申请好的。|UDF输出内存是用户自定义的，需要用户自己申请。|

## 整体开发流程

![](figures/260109145127059.png)

## 约束

无论是在host侧还是device侧，模型对输入数据都不能进行修改。
