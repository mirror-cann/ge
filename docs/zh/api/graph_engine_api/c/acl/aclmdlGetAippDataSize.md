# aclmdlGetAippDataSize

## 产品支持情况

<!-- npu="950" id972 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id972 -->
<!-- npu="A3" id973 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id973 -->
<!-- npu="910b" id974 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id974 -->
<!-- npu="310b" id975 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id975 -->
<!-- npu="310p" id976 -->
- Atlas 推理系列产品：支持
<!-- end id976 -->
<!-- npu="910" id977 -->
- Atlas 训练系列产品：支持
<!-- end id977 -->
<!-- npu="IPV350" id978 -->
- IPV350：不支持
<!-- end id978 -->

## 功能说明

根据batchSize获取动态AIPP输入数据所需的内存大小。

## 函数原型

```c
aclError aclmdlGetAippDataSize(uint64_t batchSize, size_t *size)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| batchSize | 输入 | 原始模型的batch size。 |
| size | 输出 | 内存大小，单位Byte。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
