# aclmdlSetAIPPRbuvSwapSwitch

## 产品支持情况

<!-- npu="950" id267 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id267 -->
<!-- npu="A3" id268 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id268 -->
<!-- npu="910b" id269 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id269 -->
<!-- npu="310b" id270 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id270 -->
<!-- npu="310p" id271 -->
- Atlas 推理系列产品：支持
<!-- end id271 -->
<!-- npu="910" id272 -->
- Atlas 训练系列产品：支持
<!-- end id272 -->
<!-- npu="IPV350" id273 -->
- IPV350：不支持
<!-- end id273 -->

## 功能说明

动态AIPP场景下，设置是否交换R通道与B通道、或者是否交换U通道与V通道。

## 函数原型

```c
aclError aclmdlSetAIPPRbuvSwapSwitch(aclmdlAIPP *aippParmsSet, int8_t rbuvSwapSwitch)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| rbuvSwapSwitch | 输入 | 表示是否交换R通道与B通道、或者是否交换U通道与V通道的开关，取值范围：<br><br>  - 0：不交换<br>  - 1：交换 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
