# --core\_type

## 产品支持情况

<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- npu="950" id6 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id6 -->
<!-- npu="A3" id5 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id5 -->
<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id4 -->
<!-- npu="310b" id3 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id3 -->
<!-- npu="310p" id2 -->
- Atlas 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910" id1 -->
- Atlas 训练系列产品：不支持
<!-- end id1 -->
<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--core_type_res.md#id1 -->

## 功能说明

设置模型编译时使用的Core类型。

<!-- npu="310p" id8 -->
该参数仅在Atlas 推理系列产品支持，其他产品配置后，模型转换成功，但是功能不生效。
<!-- end id8 -->

## 关联参数

无。

## 参数取值

**参数值：**

- VectorCore
- AiCore，默认为AiCore

**参数值约束：**
若网络模型中包括Cube算子，则只能使用AiCore。

## 推荐配置及收益

无。

## 示例

```bash
atc --core_type=VectorCore ...
```

## 依赖约束

无。
