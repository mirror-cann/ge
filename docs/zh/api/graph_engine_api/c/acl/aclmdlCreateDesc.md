# aclmdlCreateDesc

## 产品支持情况

<!-- npu="950" id344 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id344 -->
<!-- npu="A3" id345 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id345 -->
<!-- npu="910b" id346 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id346 -->
<!-- npu="310b" id347 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id347 -->
<!-- npu="310p" id348 -->
- Atlas 推理系列产品：支持
<!-- end id348 -->
<!-- npu="910" id349 -->
- Atlas 训练系列产品：支持
<!-- end id349 -->
<!-- npu="IPV350" id350 -->
- IPV350：支持
<!-- end id350 -->

## 功能说明

创建aclmdlDesc类型的数据，表示模型描述信息。

如需销毁aclmdlDesc类型的数据，请参见[aclmdlDestroyDesc](aclmdlDestroyDesc.md)。

## 函数原型

```c
aclmdlDesc* aclmdlCreateDesc()
```

## 参数说明

无

## 返回值说明

返回aclmdlDesc类型的指针。
