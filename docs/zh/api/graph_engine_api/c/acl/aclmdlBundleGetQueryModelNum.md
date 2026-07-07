# aclmdlBundleGetQueryModelNum

## 产品支持情况

<!-- npu="950" id134 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id134 -->
<!-- npu="A3" id135 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id135 -->
<!-- npu="910b" id136 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id136 -->
<!-- npu="310b" id137 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id137 -->
<!-- npu="310p" id138 -->
- Atlas 推理系列产品：支持
<!-- end id138 -->
<!-- npu="910" id139 -->
- Atlas 训练系列产品：支持
<!-- end id139 -->
<!-- npu="IPV350" id140 -->
- IPV350：不支持
<!-- end id140 -->

## 功能说明

根据aclmdlBundleQueryInfo查询模型中的图总数。

## 函数原型

```c
aclError aclmdlBundleGetQueryModelNum(const aclmdlBundleQueryInfo *queryInfo, size_t *modelNum)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| queryInfo | 输入 | 需提前调用[aclmdlBundleCreateQueryInfo](aclmdlBundleCreateQueryInfo.md)接口创建aclmdlBundleQueryInfo类型的数据。 |
| modelNum | 输出 | 模型中的图总数。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
