# aclopSetAttrListListInt

## 产品支持情况

<!-- npu="950" id176 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id176 -->
<!-- npu="A3" id177 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id177 -->
<!-- npu="910b" id178 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id178 -->
<!-- npu="310b" id179 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id179 -->
<!-- npu="310p" id180 -->
- Atlas 推理系列产品：支持
<!-- end id180 -->
<!-- npu="910" id181 -->
- Atlas 训练系列产品：支持
<!-- end id181 -->
<!-- npu="IPV350" id182 -->
- IPV350：不支持
<!-- end id182 -->
- MC62CM12A AI处理器：不支持

## 功能说明

设置int64\_t类型列表的属性值。

## 函数原型

```c
aclError aclopSetAttrListListInt(aclopAttr *attr,
const char *attrName,
int numLists,
const int *numValues,
const int64_t *const values[])
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr | 输出 | aclopAttr类型的指针。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| attrName | 输入 | 属性名的指针。 |
| numLists | 输入 | 列表数目。 |
| numValues | 输入 | 列表中属性值数目。<br>numValues是一个数组，数组中的每个元素表示每个列表中的属性值数目。 |
| values | 输入 | 列表中属性值的指针数组。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
