# ENABLE\_NETWORK\_ANALYSIS\_DEBUG

## 功能描述

TensorFlow训练场景下，计算图编译失败时默认会终止训练流程，不会继续向Device下发剩余的图。若开发者希望图编译失败时，不终止训练流程，允许TF Adapter持续向Device下发计算图，可通过设置该环境变量实现。

> [!NOTE]说明
>
>当前版本该环境变量取值无限制，只要环境中存在该环境变量，图编译失败时就不会终止训练流程。

## 配置示例

```bash
export ENABLE_NETWORK_ANALYSIS_DEBUG=1
```

## 使用约束

当前此环境变量仅支持TensorFlow训练场景。

## 支持的型号

<!-- npu="950" id1 -->
Ascend 950PR/Ascend 950DT
<!-- end id1 -->

<!-- npu="A3" id2 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id2 -->

<!-- npu="910b" id3 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id3 -->

<!-- npu="910" id4 -->
Atlas 训练系列产品
<!-- end id4 -->
