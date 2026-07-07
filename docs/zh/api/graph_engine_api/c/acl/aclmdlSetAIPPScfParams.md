# aclmdlSetAIPPScfParams

## 产品支持情况

<!-- npu="950" id15 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id15 -->
<!-- npu="A3" id16 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id16 -->
<!-- npu="910b" id17 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id17 -->
<!-- npu="310b" id18 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id18 -->
<!-- npu="310p" id19 -->
- Atlas 推理系列产品：支持
<!-- end id19 -->
<!-- npu="910" id20 -->
- Atlas 训练系列产品：支持
<!-- end id20 -->
<!-- npu="IPV350" id21 -->
- IPV350：不支持
<!-- end id21 -->

## 功能说明

动态AIPP场景下，设置缩放相关的参数。

## 函数原型

```c
aclError aclmdlSetAIPPScfParams(aclmdlAIPP *aippParmsSet, int8_t scfSwitch,
int32_t scfInputSizeW, int32_t scfInputSizeH,
int32_t scfOutputSizeW, int32_t scfOutputSizeH,
uint64_t batchIndex)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| scfSwitch | 输入 | 是否对图片执行缩放操作，取值范围：<br><br>  - 0：不执行缩放操作，设置为0时，则设置scfInputSizeW、scfInputSizeH、scfOutputSizeW、scfOutputSizeH参数无效<br>  - 1：执行缩放操作 |
| scfInputSizeW | 输入 | 缩放前图片的宽。<br>取值范围：[16,4096]<br>若开启了抠图功能，则缩放前图片的宽与[抠图区域的宽度](aclmdlSetAIPPCropParams.md)保持一致；若未开启抠图功能，则缩放前图片的宽与[原始图片的宽](aclmdlSetAIPPSrcImageSize.md)保持一致。 |
| scfInputSizeH | 输入 | 缩放前图片的高。<br>取值范围：[16,4096]<br>若开启了抠图功能，则缩放前图片的高与[抠图区域的高度](aclmdlSetAIPPCropParams.md)保持一致；若未开启抠图功能，则缩放前图片的高与[原始图片的高](aclmdlSetAIPPSrcImageSize.md)保持一致。 |
| scfOutputSizeW | 输入 | 缩放后图片的宽。<br>取值范围：[16,1920] |
| scfOutputSizeH | 输入 | 缩放后图片的高。<br>取值范围：[16,4096] |
| batchIndex | 输入 | 指定对第几个Batch上的图片执行缩放操作。<br>取值范围：[0,batchSize)<br>batchSize是在调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据时设置。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

缩放比例scfOutputSizeW/scfInputSizeW∈\[1/16,16\]、scfOutputSizeH/scfInputSizeH∈\[1/16,16\]。
