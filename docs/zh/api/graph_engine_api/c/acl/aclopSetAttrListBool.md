# aclopSetAttrListBool

## 产品支持情况

<!-- npu="950" id106 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id106 -->
<!-- npu="A3" id107 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id107 -->
<!-- npu="910b" id108 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id108 -->
<!-- npu="310b" id109 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id109 -->
<!-- npu="310p" id110 -->
- Atlas 推理系列产品：支持
<!-- end id110 -->
<!-- npu="910" id111 -->
- Atlas 训练系列产品：支持
<!-- end id111 -->
<!-- npu="IPV350" id112 -->
- IPV350：不支持
<!-- end id112 -->
- MC62CM12A AI处理器：不支持

## 功能说明

设置bool类型列表的属性值。

## 函数原型

```c
aclError aclopSetAttrListBool(aclopAttr *attr, const char *attrName, int numValues, const uint8_t *values)
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
