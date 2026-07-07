# 场景介绍

本节给出编译运行Graph的几种典型业务场景，用户可以根据实际情况选择需要进行的业务。

**表 1**  编译和运行Graph场景

|场景|功能说明|
|--|--|
|[编译Graph为离线模型](#编译graph为离线模型)|**将Graph编译并保存为om离线模型**，编译生成的离线模型通过模型加载接口加载，然后通过模型执行接口进行推理。详细介绍请参考[《应用开发 (C&C++)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理”章节。|
|[编译并运行Graph](#编译并运行graph)|构建完Graph之后，**直接编译并运行Graph**，使用AddGraph接口加载Graph对象；运行Graph时，根据调用接口不同，分为如下两种情况：<br>- 同步运行Graph：加载完后，使用RunGraph同步运行Graph，**得到图的执行结果**。<br>- 异步运行Graph：加载完后，使用RunGraphWithStreamAsync异步运行Graph，**得到图的执行结果**。|

说明：

- 编译Graph为离线模型：全量芯片支持。

- 编译并运行Graph，产品支持情况如下：

    <!-- npu="950" id1 -->
    Ascend 950PR/Ascend 950DT：支持
    <!-- end id1 -->

    <!-- npu="A3" id2 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
    <!-- end id2 -->

    <!-- npu="910b" id3 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
    <!-- end id3 -->

    <!-- npu="310b" id4 -->
    Atlas 200I/500 A2 推理产品：不支持
    <!-- end id4 -->

    <!-- npu="310p" id5 -->
    Atlas 推理系列产品：支持
    <!-- end id5 -->

    <!-- npu="910" id6 -->
    Atlas 训练系列产品：支持
    <!-- end id6 -->

   <!-- npu="IPV350" id7 -->
   IPV350：不支持
   <!-- end id7 -->

    <!-- @ref: ge/res/docs/zh/user_guides/graph_dev/scenario_introduction_res.md#id1 -->

## 编译Graph为离线模型

该场景把**Graph编译并保存为om离线模型**，具体流程如下图所示，详细业务介绍请参见[编译Graph为离线模型](compile_graph_to_offline_model.md)。

**图 1**  模型构建流程
![图示](../figures/model_build_flow.png "模型构建流程")

## 编译并运行Graph

完成构建Graph及Graph编译运行的具体流程如下图所示。详细业务介绍请参见[编译并运行Graph](../compile_and_run_graph/compile_and_run_graph.md)。

**图 2**  编译并运行流程图
![图示](../figures/compile_and_run_flow_diagram.png "编译并运行流程图")
