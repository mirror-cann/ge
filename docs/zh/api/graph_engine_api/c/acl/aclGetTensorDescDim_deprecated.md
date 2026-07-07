# aclGetTensorDescDim（废弃）

**须知：此接口后续版本会废弃，请使用[aclGetTensorDescDimV2](aclGetTensorDescDimV2.md)接口。**

## 产品支持情况

<!-- npu="950" id449 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id449 -->
<!-- npu="A3" id450 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id450 -->
<!-- npu="910b" id451 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id451 -->
<!-- npu="310b" id452 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id452 -->
<!-- npu="310p" id453 -->
- Atlas 推理系列产品：支持
<!-- end id453 -->
<!-- npu="910" id454 -->
- Atlas 训练系列产品：支持
<!-- end id454 -->
<!-- npu="IPV350" id455 -->
- IPV350：不支持
<!-- end id455 -->

## 功能说明

获取tensor描述中指定维度的大小。

## 函数原型

```c
int64_t aclGetTensorDescDim(const aclTensorDesc *desc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| index | 输入 | 指定获取第几个维度的大小，index值从0开始。<br>用户调用[aclGetTensorDescNumDims](aclGetTensorDescNumDims.md)接口获取shape维度个数，这个Index的取值范围：[0, (shape维度个数-1)]。 |

## 返回值说明

返回指定tensor描述中指定维度的大小。
