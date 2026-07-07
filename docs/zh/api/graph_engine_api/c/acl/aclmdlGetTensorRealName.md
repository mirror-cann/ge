# aclmdlGetTensorRealName

## 产品支持情况

<!-- npu="950" id1021 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1021 -->
<!-- npu="A3" id1022 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1022 -->
<!-- npu="910b" id1023 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1023 -->
<!-- npu="310b" id1024 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1024 -->
<!-- npu="310p" id1025 -->
- Atlas 推理系列产品：支持
<!-- end id1025 -->
<!-- npu="910" id1026 -->
- Atlas 训练系列产品：支持
<!-- end id1026 -->
<!-- npu="IPV350" id1027 -->
- IPV350：不支持
<!-- end id1027 -->

## 功能说明

根据指定名称获取tensor的真实名称。

aclmdlGetTensorRealName接口需要与[aclmdlGetInputDims](aclmdlGetInputDims.md)/[aclmdlGetInputDimsV2](aclmdlGetInputDimsV2.md)/[aclmdlGetOutputDims](aclmdlGetOutputDims.md)/[aclmdlGetCurOutputDims](aclmdlGetCurOutputDims.md)接口配合使用。

## 函数原型

```c
const char *aclmdlGetTensorRealName(const aclmdlDesc *modelDesc, const char *name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| name | 输入 | 名称的指针，用于根据该名称获取tensor的真实名称。 |

## 返回值说明

返回指向tensor真实名称的指针，该指针的生命周期与modelDesc相同，若modelDesc资源被销毁，则该指针指向的内容也会自动被销毁。

若modelDesc或name为空，则返回nullptr。
