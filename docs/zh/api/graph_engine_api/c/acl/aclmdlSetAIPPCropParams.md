# aclmdlSetAIPPCropParams

## 产品支持情况

<!-- npu="950" id720 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id720 -->
<!-- npu="A3" id721 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id721 -->
<!-- npu="910b" id722 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id722 -->
<!-- npu="310b" id723 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id723 -->
<!-- npu="310p" id724 -->
- Atlas 推理系列产品：支持
<!-- end id724 -->
<!-- npu="910" id725 -->
- Atlas 训练系列产品：支持
<!-- end id725 -->
<!-- npu="IPV350" id726 -->
- IPV350：不支持
<!-- end id726 -->

## 功能说明

动态AIPP场景下，设置抠图相关的参数。

## 函数原型

```c
aclError aclmdlSetAIPPCropParams(aclmdlAIPP *aippParmsSet, int8_t cropSwitch,
int32_t cropStartPosW, int32_t cropStartPosH,
int32_t cropSizeW, int32_t cropSizeH,
uint64_t batchIndex)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| cropSwitch | 输入 | 是否对图片执行抠图操作，取值范围：<br><br>  - 0：不执行抠图操作，设置为0时，则设置cropStartPosW、cropStartPosH、cropSizeW、cropSizeH参数无效<br>  - 1：执行抠图操作 |
| cropStartPosW | 输入 | 抠图时，坐标点起始位置在图中横向的坐标。<br>对于YUV420SP_U8格式的图像，参数取值要求是偶数。<br>取值范围：[0,4095] |
| cropStartPosH | 输入 | 抠图时，坐标点起始位置在图中纵向的坐标。<br>对于YUV420SP_U8格式的图像，参数取值要求是偶数。<br>取值范围：[0,4095] |
| cropSizeW | 输入 | 抠图区域的宽度。<br>取值范围：[1,4096] |
| cropSizeH | 输入 | 抠图区域的高度。<br>取值范围：[1,4096] |
| batchIndex | 输入 | 指定对第几个Batch上的图片执行抠图操作。<br>取值范围：[0,batchSize)<br>batchSize是在调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据时设置。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

若开启抠图功能，则通过[aclmdlSetAIPPSrcImageSize](aclmdlSetAIPPSrcImageSize.md)接口设置的参数与通过aclmdlSetAIPPCropParams接口设置的参数之间必须满足以下公式：

- cropSizeW+cropStartPosW ≤ srcImageSizeW
- cropSizeH+cropStartPosH ≤ srcImageSizeH
