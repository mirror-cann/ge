# aclopSetAttrListDataType

## 产品支持情况

<!-- npu="950" id442 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id442 -->
<!-- npu="A3" id443 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id443 -->
<!-- npu="910b" id444 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id444 -->
<!-- npu="310b" id445 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id445 -->
<!-- npu="310p" id446 -->
- Atlas 推理系列产品：支持
<!-- end id446 -->
<!-- npu="910" id447 -->
- Atlas 训练系列产品：支持
<!-- end id447 -->
<!-- npu="IPV350" id448 -->
- IPV350：不支持
<!-- end id448 -->
- MC62CM12A AI处理器：不支持

## 功能说明

设置指定属性列表的数据类型。

## 函数原型

```c
aclError aclopSetAttrListDataType(aclopAttr *attr, const char *attrName, int numValues, const aclDataType values[])
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr | 输出 | aclopAttr类型的指针。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| attrName | 输入 | 属性名的指针。 |
| numValues | 输入 | 属性值数目。 |
| values | 输入 | 属性值的数组。类型定义请参见aclDataType。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
