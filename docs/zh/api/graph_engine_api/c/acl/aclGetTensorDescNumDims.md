# aclGetTensorDescNumDims

## 产品支持情况

<!-- npu="950" id594 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id594 -->
<!-- npu="A3" id595 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id595 -->
<!-- npu="910b" id596 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id596 -->
<!-- npu="310b" id597 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id597 -->
<!-- npu="310p" id598 -->
- Atlas 推理系列产品：支持
<!-- end id598 -->
<!-- npu="910" id599 -->
- Atlas 训练系列产品：支持
<!-- end id599 -->
<!-- npu="IPV350" id600 -->
- IPV350：不支持
<!-- end id600 -->

## 功能说明

获取tensor描述中shape的维度个数。

## 函数原型

```c
size_t aclGetTensorDescNumDims(const aclTensorDesc *desc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |

## 返回值说明

返回tensor描述中shape的维度个数。若返回ACL\_UNKNOWN\_RANK（\#define ACL\_UNKNOWN\_RANK 0xFFFFFFFFFFFFFFFE）表示动态Shape场景下维度个数未知。
