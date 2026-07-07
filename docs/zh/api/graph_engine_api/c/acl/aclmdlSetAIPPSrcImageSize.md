# aclmdlSetAIPPSrcImageSize

## 产品支持情况

<!-- npu="950" id1119 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1119 -->
<!-- npu="A3" id1120 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1120 -->
<!-- npu="910b" id1121 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1121 -->
<!-- npu="310b" id1122 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1122 -->
<!-- npu="310p" id1123 -->
- Atlas 推理系列产品：支持
<!-- end id1123 -->
<!-- npu="910" id1124 -->
- Atlas 训练系列产品：支持
<!-- end id1124 -->
<!-- npu="IPV350" id1125 -->
- IPV350：不支持
<!-- end id1125 -->

## 功能说明

动态AIPP场景下，必须设置原始图片的宽和高。

## 函数原型

```c
aclError aclmdlSetAIPPSrcImageSize(aclmdlAIPP *aippParmsSet, int32_t srcImageSizeW, int32_t srcImageSizeH)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| srcImageSizeW | 输入 | 原始图片的宽，对于YUV420SP_U8或YUV422SP_U8或YUYV_U8类型的图像，要求srcImageSizeW取值是偶数。<br>取值范围： [2,4096] |
| srcImageSizeH | 输入 | 原始图片的高，对于YUV420SP_U8类型的图像，要求srcImageSizeH取值是偶数。<br>取值范围： [1,4096] |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
