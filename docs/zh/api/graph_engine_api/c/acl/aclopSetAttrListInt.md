# aclopSetAttrListInt

## 产品支持情况

<!-- npu="950" id1091 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1091 -->
<!-- npu="A3" id1092 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1092 -->
<!-- npu="910b" id1093 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1093 -->
<!-- npu="310b" id1094 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1094 -->
<!-- npu="310p" id1095 -->
- Atlas 推理系列产品：支持
<!-- end id1095 -->
<!-- npu="910" id1096 -->
- Atlas 训练系列产品：支持
<!-- end id1096 -->
<!-- npu="IPV350" id1097 -->
- IPV350：不支持
<!-- end id1097 -->
- MC62CM12A AI处理器：不支持

## 功能说明

设置int64\_t类型列表的属性值。

## 函数原型

```c
aclError aclopSetAttrListInt(aclopAttr *attr, const char *attrName, int numValues, const int64_t *values)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr | 输出 | aclopAttr类型的指针。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| attrName | 输入 | 属性名的指针。 |
| numValues | 输入 | 属性值数目。 |
| values | 输入 | 属性值的数组。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
