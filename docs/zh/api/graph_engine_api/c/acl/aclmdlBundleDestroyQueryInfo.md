# aclmdlBundleDestroyQueryInfo

## 产品支持情况

<!-- npu="950" id351 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id351 -->
<!-- npu="A3" id352 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id352 -->
<!-- npu="910b" id353 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id353 -->
<!-- npu="310b" id354 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id354 -->
<!-- npu="310p" id355 -->
- Atlas 推理系列产品：支持
<!-- end id355 -->
<!-- npu="910" id356 -->
- Atlas 训练系列产品：支持
<!-- end id356 -->
<!-- npu="IPV350" id357 -->
- IPV350：不支持
<!-- end id357 -->

## 功能说明

销毁通过[aclmdlBundleCreateQueryInfo](aclmdlBundleCreateQueryInfo.md)接口创建的aclmdlBundleQueryInfo类型的数据。

## 函数原型

```c
aclError aclmdlBundleDestroyQueryInfo(aclmdlBundleQueryInfo *queryInfo)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| queryInfo | 输入 | 待销毁的aclmdlBundleQueryInfo类型的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
