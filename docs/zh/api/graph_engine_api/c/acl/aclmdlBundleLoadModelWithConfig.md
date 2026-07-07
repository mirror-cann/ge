# aclmdlBundleLoadModelWithConfig

## 产品支持情况

<!-- npu="950" id491 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id491 -->
<!-- npu="A3" id492 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id492 -->
<!-- npu="910b" id493 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id493 -->
<!-- npu="310b" id494 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id494 -->
<!-- npu="310p" id495 -->
- Atlas 推理系列产品：支持
<!-- end id495 -->
<!-- npu="910" id496 -->
- Atlas 训练系列产品：支持
<!-- end id496 -->
<!-- npu="IPV350" id497 -->
- IPV350：不支持
<!-- end id497 -->

## 功能说明

根据图索引加载模型中的图，同时可以指定加载配置信息。

## 函数原型

```c
aclError aclmdlBundleLoadModelWithConfig(uint32_t bundleId, size_t index, aclmdlConfigHandle *handle, uint32_t *modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| bundleId | 输入 | 通过[aclmdlBundleInitFromFile](aclmdlBundleInitFromFile.md)或者[aclmdlBundleInitFromMem](aclmdlBundleInitFromMem.md)接口初始化模型成功后返回的bundleId。 |
| index | 输入 | 索引。<br>用户调用[aclmdlBundleGetQueryModelNum](aclmdlBundleGetQueryModelNum.md)接口获取图总数后，这个index的取值范围：[0, (图总数-1)]。 |
| handle | 输入 | 模型加载的配置对象的指针。需提前调用[aclmdlCreateConfigHandle](aclmdlCreateConfigHandle.md)接口创建该对象，与aclmdlSetConfigOpt中的handle保持一致。 |
| modelId | 输出 | 实际可执行的图ID。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
