# aclmdlBundleUnload

## 产品支持情况

<!-- npu="950" id839 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id839 -->
<!-- npu="A3" id840 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id840 -->
<!-- npu="910b" id841 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id841 -->
<!-- npu="310b" id842 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id842 -->
<!-- npu="310p" id843 -->
- Atlas 推理系列产品：支持
<!-- end id843 -->
<!-- npu="910" id844 -->
- Atlas 训练系列产品：支持
<!-- end id844 -->
<!-- npu="IPV350" id845 -->
- IPV350：不支持
<!-- end id845 -->

## 功能说明

系统完成模型推理后，可调用该接口卸载通过[aclmdlBundleLoadFromFile](aclmdlBundleLoadFromFile.md)接口或[aclmdlBundleLoadFromMem](aclmdlBundleLoadFromMem.md)接口加载的模型，释放资源。

**本接口需与其它接口配合使用**，以便实现动态更新变量的目的，请参见[aclmdlBundleLoadFromFile](aclmdlBundleLoadFromFile.md)处的说明。

## 函数原型

```c
aclError aclmdlBundleUnload(uint32_t bundleId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| bundleId | 输入 | 通过[aclmdlBundleLoadFromFile](aclmdlBundleLoadFromFile.md)接口或[aclmdlBundleLoadFromMem](aclmdlBundleLoadFromMem.md)接口加载模型成功后返回的bundleId。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
