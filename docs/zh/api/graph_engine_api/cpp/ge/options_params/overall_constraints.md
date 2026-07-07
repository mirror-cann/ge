# 总体约束

**本章节参数产品支持情况汇总如下：若参数中又标注了具体支持的产品，请以参数中实际支持的为准。**

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
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/options_params/overall_constraints_res.md#id1 -->

**本章节列出GEInitialize、Session构造函数、AddGraph接口传入的配置参数，分别在全局、session、graph级别生效**。

> [!NOTE]说明
>
>[基础功能](./basic_functions.md)\~[后续版本废弃配置](./deprecated_config_for_future_versions.md)中仅列出当前版本支持的配置参数，如果表中未列出，表示该参数预留或适用于其他版本的AI处理器，用户无需关注。
