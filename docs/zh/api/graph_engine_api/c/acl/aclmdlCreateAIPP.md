# aclmdlCreateAIPP

## 产品支持情况

<!-- npu="950" id29 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id29 -->
<!-- npu="A3" id30 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id30 -->
<!-- npu="910b" id31 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id31 -->
<!-- npu="310b" id32 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id32 -->
<!-- npu="310p" id33 -->
- Atlas 推理系列产品：支持
<!-- end id33 -->
<!-- npu="910" id34 -->
- Atlas 训练系列产品：支持
<!-- end id34 -->
<!-- npu="IPV350" id35 -->
- IPV350：不支持
<!-- end id35 -->

## 功能说明

动态AIPP场景下，根据模型支持的batch size创建aclmdlAIPP类型的数据，用于存放动态AIPP的参数。

如需销毁aclmdlAIPP类型的数据，请参见[aclmdlDestroyAIPP](aclmdlDestroyAIPP.md)。

## 函数原型

```c
aclmdlAIPP *aclmdlCreateAIPP(uint64_t batchSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| batchSize | 输入 | 原始模型的batch size。 |

## 返回值说明

返回aclmdlAIPP类型的指针。
