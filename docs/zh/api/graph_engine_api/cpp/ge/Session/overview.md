# 简介

[Session](Session.md)（原有接口，也称V1版本）与[GESession](../GeSession/GESession.md)（V2版本）都是图编译执行的接口，用于实现图加载、图编译、图执行的功能，但两套接口不能混用，建议使用V2版本接口，V1版本接口后续会逐步废弃，Session到GeSession的迁移指导请参见[Session到GeSession的迁移指导](../../../../../user_guides/graph_dev/appendix/session_to_gesession_migration.md)。

- 原有Session中同时包含GE与DataFlow的接口，现将GE相关接口拆分至ge\_api\_v2.h头文件中的GeSession类中，以实现更清晰的模块划分。
- 此外，针对原有接口中存在的命名相近但功能差异较大的问题，进行了统一优化：原编译接口包括CompileGraph与BuildGraph，执行接口包括ExecuteGraphWithStreamAsync与RunGraphWithStreamAsync，这些接口在功能上存在明确区别，但命名易造成混淆，且存在配合使用的限制：例如，BuildGraph无法与ExecuteGraphWithStreamAsync配合使用，CompileGraph也无法与RunGraph/RunGraphAsync配套调用。

在V2版本中，对接口进行了精简与统一：

- 编译接口仅保留一个：CompileGraph。
- 带流异步执行接口也统一为一个：RunGraphWithStreamAsync。
- 执行接口中的inputs与outputs参数统一采用性能更优的gert::Tensor类型。

该调整旨在提升接口的清晰度与使用一致性，避免因接口误用导致功能异常。

**Session接口产品支持情况如下：**

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/Session/overview_res.md#id1 -->
