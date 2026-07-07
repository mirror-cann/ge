# aclmdlSetAIPPDtcPixelMin

## 产品支持情况

<!-- npu="950" id1217 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1217 -->
<!-- npu="A3" id1218 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1218 -->
<!-- npu="910b" id1219 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1219 -->
<!-- npu="310b" id1220 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1220 -->
<!-- npu="310p" id1221 -->
- Atlas 推理系列产品：支持
<!-- end id1221 -->
<!-- npu="910" id1222 -->
- Atlas 训练系列产品：支持
<!-- end id1222 -->
<!-- npu="IPV350" id1223 -->
- IPV350：不支持
<!-- end id1223 -->

## 功能说明

动态AIPP场景下，设置通道的最小值。

## 函数原型

```c
aclError aclmdlSetAIPPDtcPixelMin(aclmdlAIPP *aippParmsSet,
float dtcPixelMinChn0,
float dtcPixelMinChn1,
float dtcPixelMinChn2,
float dtcPixelMinChn3,
uint64_t batchIndex)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| dtcPixelMinChn0 | 输入 | 通道0的最小值。<br>取值范围：[0, 255] |
| dtcPixelMinChn1 | 输入 | 通道1的最小值。<br>取值范围：[0, 255] |
| dtcPixelMinChn2 | 输入 | 通道2的最小值。<br>取值范围：[0, 255] |
| dtcPixelMinChn3 | 输入 | 通道3的最小值。如果只有3个通道，将该参数设置为0。<br>取值范围：[0, 255] |
| batchIndex | 输入 | 指定对第几个Batch上的图片设置通道最小值。<br>取值范围：[0,batchSize)<br>batchSize是在调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据时设置。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
