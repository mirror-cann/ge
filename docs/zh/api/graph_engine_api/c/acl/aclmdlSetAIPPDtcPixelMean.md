# aclmdlSetAIPPDtcPixelMean

## 产品支持情况

<!-- npu="950" id790 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id790 -->
<!-- npu="A3" id791 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id791 -->
<!-- npu="910b" id792 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id792 -->
<!-- npu="310b" id793 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id793 -->
<!-- npu="310p" id794 -->
- Atlas 推理系列产品：支持
<!-- end id794 -->
<!-- npu="910" id795 -->
- Atlas 训练系列产品：支持
<!-- end id795 -->
<!-- npu="IPV350" id796 -->
- IPV350：不支持
<!-- end id796 -->

## 功能说明

动态AIPP场景下，设置通道的均值。

## 函数原型

```c
aclError aclmdlSetAIPPDtcPixelMean(aclmdlAIPP *aippParmsSet,
int16_t dtcPixelMeanChn0,
int16_t dtcPixelMeanChn1,
int16_t dtcPixelMeanChn2,
int16_t dtcPixelMeanChn3,
uint64_t batchIndex)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| dtcPixelMeanChn0 | 输入 | 通道0的均值。<br>取值范围：[0, 255] |
| dtcPixelMeanChn1 | 输入 | 通道1的均值。<br>取值范围：[0, 255] |
| dtcPixelMeanChn2 | 输入 | 通道2的均值。<br>取值范围：[0, 255] |
| dtcPixelMeanChn3 | 输入 | 通道3的均值。<br>如果只有3个通道，将该参数设置为0。<br>取值范围：[0, 255] |
| batchIndex | 输入 | 指定对第几个Batch上的图片设置通道均值。<br>取值范围：[0,batchSize)<br>batchSize是在调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据时设置。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
