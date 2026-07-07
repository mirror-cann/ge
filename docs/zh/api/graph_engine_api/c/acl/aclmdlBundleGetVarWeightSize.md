# aclmdlBundleGetVarWeightSize

## 产品支持情况

<!-- npu="950" id1056 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1056 -->
<!-- npu="A3" id1057 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1057 -->
<!-- npu="910b" id1058 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1058 -->
<!-- npu="310b" id1059 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1059 -->
<!-- npu="310p" id1060 -->
- Atlas 推理系列产品：支持
<!-- end id1060 -->
<!-- npu="910" id1061 -->
- Atlas 训练系列产品：支持
<!-- end id1061 -->
<!-- npu="IPV350" id1062 -->
- IPV350：不支持
<!-- end id1062 -->

## 功能说明

获取模型的可更新权重内存大小。

## 函数原型

```c
aclError aclmdlBundleGetVarWeightSize(const aclmdlBundleQueryInfo *queryInfo, size_t *variableWeightSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| queryInfo | 输入 | 需提前调用[aclmdlBundleCreateQueryInfo](aclmdlBundleCreateQueryInfo.md)接口创建aclmdlBundleQueryInfo类型的数据。 |
| variableWeightSize | 输出 | 获取模型的可更新权重内存大小。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
