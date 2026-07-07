# aclSetTensorPlaceMent

## 产品支持情况

<!-- npu="950" id435 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id435 -->
<!-- npu="A3" id436 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id436 -->
<!-- npu="910b" id437 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id437 -->
<!-- npu="310b" id438 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id438 -->
<!-- npu="310p" id439 -->
- Atlas 推理系列产品：支持
<!-- end id439 -->
<!-- npu="910" id440 -->
- Atlas 训练系列产品：支持
<!-- end id440 -->
<!-- npu="IPV350" id441 -->
- IPV350：不支持
<!-- end id441 -->

## 功能说明

设置TensorDesc的placement属性，用于标识存放Tensor数据的内存类型。

## 函数原型

```c
aclError aclSetTensorPlaceMent(aclTensorDesc *desc, aclMemType memType)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| memType | 输入 | 指定placement属性值。类型定义请参见aclMemType。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
