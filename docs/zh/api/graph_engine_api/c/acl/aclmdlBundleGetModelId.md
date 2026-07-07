# aclmdlBundleGetModelId

## 产品支持情况

<!-- npu="950" id769 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id769 -->
<!-- npu="A3" id770 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id770 -->
<!-- npu="910b" id771 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id771 -->
<!-- npu="310b" id772 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id772 -->
<!-- npu="310p" id773 -->
- Atlas 推理系列产品：支持
<!-- end id773 -->
<!-- npu="910" id774 -->
- Atlas 训练系列产品：支持
<!-- end id774 -->
<!-- npu="IPV350" id775 -->
- IPV350：不支持
<!-- end id775 -->

## 功能说明

获取实际可执行的模型ID。

**本接口需与其它接口配合使用**，以便实现动态更新变量的目的，请参见[aclmdlBundleLoadFromFile](aclmdlBundleLoadFromFile.md)处的说明。

## 函数原型

```c
aclError aclmdlBundleGetModelId(uint32_t bundleId, size_t index, uint32_t *modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| bundleId | 输入 | 通过[aclmdlBundleLoadFromFile](aclmdlBundleLoadFromFile.md)接口或[aclmdlBundleLoadFromMem](aclmdlBundleLoadFromMem.md)接口加载模型成功后返回的bundleId。 |
| index | 输入 | 索引。<br>用户调用[aclmdlBundleGetModelNum](aclmdlBundleGetModelNum.md)接口获取模型数量后，这个index的取值范围：[0, (模型数量-1)]。 |
| modelId | 输出 | 实际可执行的模型ID。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
