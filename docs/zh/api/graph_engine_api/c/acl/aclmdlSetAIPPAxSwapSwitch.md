# aclmdlSetAIPPAxSwapSwitch

## 产品支持情况

<!-- npu="950" id909 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id909 -->
<!-- npu="A3" id910 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id910 -->
<!-- npu="910b" id911 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id911 -->
<!-- npu="310b" id912 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id912 -->
<!-- npu="310p" id913 -->
- Atlas 推理系列产品：支持
<!-- end id913 -->
<!-- npu="910" id914 -->
- Atlas 训练系列产品：支持
<!-- end id914 -->
<!-- npu="IPV350" id915 -->
- IPV350：不支持
<!-- end id915 -->

## 功能说明

动态AIPP场景下，设置RGBA-\>ARGB或者YUVA-\>AYUV的交换开关。

## 函数原型

```c
aclError aclmdlSetAIPPAxSwapSwitch(aclmdlAIPP *aippParmsSet, int8_t axSwapSwitch)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| axSwapSwitch | 输入 | 表示RGBA->ARGB或者YUVA->AYUV的交换开关，取值范围：<br><br>  - 0：不交换<br>  - 1：交换 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
