# --enable\_single\_stream

## 产品支持情况

<!-- npu="950" id6 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id6 -->
<!-- npu="A3" id5 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id4 -->
<!-- npu="310b" id3 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id3 -->
<!-- npu="310p" id2 -->
- Atlas 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910" id1 -->
- Atlas 训练系列产品：支持
<!-- end id1 -->

<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--enable_single_stream_res.md#id1 -->

## 功能说明

静态shape场景下，是否启用模型推理顺序单流串行执行。

其中，流（Stream）用于维护一些异步操作的执行顺序，确保按照应用程序中的代码调用顺序在Device上执行。

## 关联参数

无。

## 参数取值

**参数值：**

- true：表示启用，模型推理顺序单流串行执行。
- false：（默认值）表示关闭，模型推理时多流并行执行。

**参数值约束：**

模型中存在Cmo算子和如下控制类算子时，不能使用单Stream特性，只能使用默认值false。

- Merge
- Switch
- Enter
- RefEnter

## 推荐配置及收益

无。

## 示例

```bash
atc --enable_single_stream=true ...
```

## 使用约束

无。
