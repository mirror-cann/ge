# aclSetTensorStorageShape（废弃）

**须知：此接口后续版本会废弃，请使用[aclSetTensorShape](aclSetTensorShape.md)接口。**

## 产品支持情况

<!-- npu="950" id539 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id539 -->
<!-- npu="A3" id540 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id540 -->
<!-- npu="910b" id541 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id541 -->
<!-- npu="310b" id542 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id542 -->
<!-- npu="310p" id543 -->
- Atlas 推理系列产品：支持
<!-- end id543 -->
<!-- npu="910" id544 -->
- Atlas 训练系列产品：x
<!-- end id544 -->
<!-- npu="IPV350" id545 -->
- IPV350：x
<!-- end id545 -->

## 功能说明

调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建tensor描述信息后，可通过aclSetTensorStorageShape接口设置tensor的实际dims信息。

## 函数原型

```c
aclError aclSetTensorStorageShape(aclTensorDesc *desc, int numDims, const int64_t *dims)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| numDims | 输入 | 需要设置的dims维度数。 |
| dims | 输入 | dims的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
