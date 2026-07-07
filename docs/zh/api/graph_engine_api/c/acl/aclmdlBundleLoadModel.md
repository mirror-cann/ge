# aclmdlBundleLoadModel

## 产品支持情况

<!-- npu="950" id629 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id629 -->
<!-- npu="A3" id630 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id630 -->
<!-- npu="910b" id631 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id631 -->
<!-- npu="310b" id632 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id632 -->
<!-- npu="310p" id633 -->
- Atlas 推理系列产品：支持
<!-- end id633 -->
<!-- npu="910" id634 -->
- Atlas 训练系列产品：支持
<!-- end id634 -->
<!-- npu="IPV350" id635 -->
- IPV350：不支持
<!-- end id635 -->

## 功能说明

根据图索引加载模型中的图，由系统内部管理内存。

## 函数原型

```c
aclError aclmdlBundleLoadModel(uint32_t bundleId, size_t index, uint32_t *modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| bundleId | 输入 | 通过[aclmdlBundleInitFromFile](aclmdlBundleInitFromFile.md)或者[aclmdlBundleInitFromMem](aclmdlBundleInitFromMem.md)接口初始化模型成功后返回的bundleId。 |
| index | 输入 | 索引。<br>用户调用[aclmdlBundleGetQueryModelNum](aclmdlBundleGetQueryModelNum.md)接口获取图总数后，这个index的取值范围：[0, (图总数-1)]。 |
| modelId | 输出 | 实际可执行的图ID。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
