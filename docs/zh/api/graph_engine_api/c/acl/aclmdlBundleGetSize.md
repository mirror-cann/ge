# aclmdlBundleGetSize

## 产品支持情况

<!-- npu="950" id888 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id888 -->
<!-- npu="A3" id889 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id889 -->
<!-- npu="910b" id890 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id890 -->
<!-- npu="310b" id891 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id891 -->
<!-- npu="310p" id892 -->
- Atlas 推理系列产品：支持
<!-- end id892 -->
<!-- npu="910" id893 -->
- Atlas 训练系列产品：支持
<!-- end id893 -->
<!-- npu="IPV350" id894 -->
- IPV350：不支持
<!-- end id894 -->

## 功能说明

根据模型描述信息和图索引获取图执行时所需的权值内存大小、工作内存大小。

## 函数原型

```c
aclError aclmdlBundleGetSize(const aclmdlBundleQueryInfo *queryInfo, size_t index, size_t *workSize, size_t *constWeightSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| queryInfo | 输入 | 需提前调用[aclmdlBundleCreateQueryInfo](aclmdlBundleCreateQueryInfo.md)接口创建aclmdlBundleQueryInfo类型的数据。 |
| index | 输入 | 索引。<br>用户调用[aclmdlBundleGetQueryModelNum](aclmdlBundleGetQueryModelNum.md)接口获取模型中的图总数后，这个index的取值范围：[0, (图总数-1)]。 |
| workSize | 输出 | 图执行时所需的工作内存大小的指针，单位Byte。<br>此处的内存为Device内存，而且需要用户申请和释放。 |
| weightSize | 输出 | 图执行时所需权值内存大小的指针，单位Byte。<br>此处的内存为Device内存，而且需要用户申请和释放。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
