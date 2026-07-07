# aclmdlDestroyAIPP

## 产品支持情况

<!-- npu="950" id832 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id832 -->
<!-- npu="A3" id833 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id833 -->
<!-- npu="910b" id834 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id834 -->
<!-- npu="310b" id835 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id835 -->
<!-- npu="310p" id836 -->
- Atlas 推理系列产品：支持
<!-- end id836 -->
<!-- npu="910" id837 -->
- Atlas 训练系列产品：支持
<!-- end id837 -->
<!-- npu="IPV350" id838 -->
- IPV350：不支持
<!-- end id838 -->

## 功能说明

销毁通过[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建的aclmdlAIPP类型的数据。

## 函数原型

```c
aclError aclmdlDestroyAIPP(const aclmdlAIPP *aippParmsSet)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输入 | 待销毁的aclmdlAIPP类型的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
