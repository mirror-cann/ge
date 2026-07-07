# aclmdlSetAIPPPixelVarReci

## 产品支持情况

<!-- npu="950" id225 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id225 -->
<!-- npu="A3" id226 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id226 -->
<!-- npu="910b" id227 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id227 -->
<!-- npu="310b" id228 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id228 -->
<!-- npu="310p" id229 -->
- Atlas 推理系列产品：支持
<!-- end id229 -->
<!-- npu="910" id230 -->
- Atlas 训练系列产品：支持
<!-- end id230 -->
<!-- npu="IPV350" id231 -->
- IPV350：不支持
<!-- end id231 -->

## 功能说明

动态AIPP场景下，设置通道的方差。

## 函数原型

```c
aclError aclmdlSetAIPPPixelVarReci(aclmdlAIPP *aippParmsSet,
float dtcPixelVarReciChn0,
float dtcPixelVarReciChn1,
float dtcPixelVarReciChn2,
float dtcPixelVarReciChn3,
uint64_t batchIndex)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| dtcPixelVarReciChn0 | 输入 | 通道0的方差的倒数，默认值为1.0。<br>取值范围：[-65504, 65504] |
| dtcPixelVarReciChn1 | 输入 | 通道1的方差的倒数，默认值为1.0。<br>取值范围：[-65504, 65504] |
| dtcPixelVarReciChn2 | 输入 | 通道2的方差的倒数，默认值为1.0。<br>取值范围：[-65504, 65504] |
| dtcPixelVarReciChn3 | 输入 | 通道3的方差的倒数，默认值为1.0。如果只有3个通道，则将该参数设置为1.0。<br>取值范围：[-65504, 65504] |
| batchIndex | 输入 | 指定对第几个Batch上的图片设置通道的方差。<br>取值范围：[0,batchSize)<br>batchSize是在调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据时设置。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
