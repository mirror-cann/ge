# aclGetTensorDescDimRange

## 产品支持情况

<!-- npu="950" id692 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id692 -->
<!-- npu="A3" id693 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id693 -->
<!-- npu="910b" id694 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id694 -->
<!-- npu="310b" id695 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id695 -->
<!-- npu="310p" id696 -->
- Atlas 推理系列产品：支持
<!-- end id696 -->
<!-- npu="910" id697 -->
- Atlas 训练系列产品：支持
<!-- end id697 -->
<!-- npu="IPV350" id698 -->
- IPV350：不支持
<!-- end id698 -->

## 功能说明

获取tensor描述中指定维度的范围，\[1,-1\]表示全shape范围。

当[aclGetTensorDescNumDims](aclGetTensorDescNumDims.md)接口的返回值为ACL\_UNKNOWN\_RANK时，表示动态Shape场景下维度个数未知，则不能调用aclGetTensorDescDimRange接口获取指定维度的范围。

## 函数原型

```c
aclError aclGetTensorDescDimRange(const aclTensorDesc *desc, size_t index, size_t dimRangeNum, int64_t *dimRange)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| index | 输入 | 指定获取第几个维度的大小，index值从0开始。<br>用户调用[aclGetTensorDescNumDims](aclGetTensorDescNumDims.md)接口获取shape维度个数，这个Index的取值范围：[0, (shape维度个数-1)]。 |
| dimRangeNum | 输入 | dimRange的长度，该值必须大于等于2。 |
| dimRange | 输出 | tensor描述中index指定维度的Shape范围。<br>dimRange是一个数组，数组的第一个元素值表示Shape范围的最小值，第二个元素值表示Shape范围的最大值。该数组中仅前2个元素值有效。<br>当dimRange数组的值为[1,-1]时，表示全Shape范围。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
