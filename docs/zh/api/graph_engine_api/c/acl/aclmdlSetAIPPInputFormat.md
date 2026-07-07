# aclmdlSetAIPPInputFormat

## 产品支持情况

<!-- npu="950" id622 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id622 -->
<!-- npu="A3" id623 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id623 -->
<!-- npu="910b" id624 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id624 -->
<!-- npu="310b" id625 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id625 -->
<!-- npu="310p" id626 -->
- Atlas 推理系列产品：支持
<!-- end id626 -->
<!-- npu="910" id627 -->
- Atlas 训练系列产品：支持
<!-- end id627 -->
<!-- npu="IPV350" id628 -->
- IPV350：不支持
<!-- end id628 -->

## 功能说明

动态AIPP场景下，必须设置原始输入图像的格式。

## 函数原型

```c
aclError aclmdlSetAIPPInputFormat(aclmdlAIPP *aippParmsSet, aclAippInputFormat inputFormat)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| inputFormat | 输入 | 表示原始输入图像的格式。类型定义请参见[aclAippInputFormat](aclAippInputFormat.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
