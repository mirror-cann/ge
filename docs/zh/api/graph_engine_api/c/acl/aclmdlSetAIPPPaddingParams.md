# aclmdlSetAIPPPaddingParams

## 产品支持情况

<!-- npu="950" id937 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id937 -->
<!-- npu="A3" id938 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id938 -->
<!-- npu="910b" id939 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id939 -->
<!-- npu="310b" id940 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id940 -->
<!-- npu="310p" id941 -->
- Atlas 推理系列产品：支持
<!-- end id941 -->
<!-- npu="910" id942 -->
- Atlas 训练系列产品：支持
<!-- end id942 -->
<!-- npu="IPV350" id943 -->
- IPV350：不支持
<!-- end id943 -->

## 功能说明

动态AIPP场景下，设置补边相关的参数。

## 函数原型

```c
aclError aclmdlSetAIPPPaddingParams(aclmdlAIPP *aippParmsSet, int8_t paddingSwitch,
int32_t paddingSizeTop, int32_t paddingSizeBottom,
int32_t paddingSizeLeft, int32_t paddingSizeRight,
uint64_t batchIndex)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| paddingSwitch | 输入 | 是否对图片执行补边操作，取值范围：<br><br>  - 0：不执行补边操作，设置为0时，则设置paddingSizeTop、paddingSizeBottom、paddingSizeLeft、paddingSizeRight参数无效<br>  - 1：执行补边操作 |
| paddingSizeTop | 输入 | 在图片上方填充的值。<br>取值范围：[0, 32] |
| paddingSizeBottom | 输入 | 在图片下方填充的值。<br>取值范围：[0, 32] |
| paddingSizeLeft | 输入 | 在图片左方填充的值。<br>取值范围：[0, 32] |
| paddingSizeRight | 输入 | 在图片右方填充的值。<br>取值范围：[0, 32] |
| batchIndex | 输入 | 指定对第几个Batch上的图片执行补边操作。<br>取值范围：[0,batchSize)<br>batchSize是在调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据时设置。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

补边之后，图片的宽必须小于等于1080。
