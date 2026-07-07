# aclmdlBundleUnloadModel

## 产品支持情况

<!-- npu="950" id860 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id860 -->
<!-- npu="A3" id861 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id861 -->
<!-- npu="910b" id862 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id862 -->
<!-- npu="310b" id863 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id863 -->
<!-- npu="310p" id864 -->
- Atlas 推理系列产品：支持
<!-- end id864 -->
<!-- npu="910" id865 -->
- Atlas 训练系列产品：支持
<!-- end id865 -->
<!-- npu="IPV350" id866 -->
- IPV350：不支持
<!-- end id866 -->

## 功能说明

卸载模型中的图。

## 函数原型

```c
aclError aclmdlBundleUnloadModel(uint32_t bundleId, uint32_t modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| bundleId | 输入 | 通过[aclmdlBundleInitFromFile](aclmdlBundleInitFromFile.md)或者[aclmdlBundleInitFromMem](aclmdlBundleInitFromMem.md)接口初始化模型成功后返回的bundleId。 |
| modelId | 输入 | 需卸载的图ID。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
