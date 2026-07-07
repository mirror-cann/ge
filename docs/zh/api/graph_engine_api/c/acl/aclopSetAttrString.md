# aclopSetAttrString

## 产品支持情况

<!-- npu="950" id567 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id567 -->
<!-- npu="A3" id568 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id568 -->
<!-- npu="910b" id569 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id569 -->
<!-- npu="310b" id570 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id570 -->
<!-- npu="310p" id571 -->
- Atlas 推理系列产品：支持
<!-- end id571 -->
<!-- npu="910" id572 -->
- Atlas 训练系列产品：支持
<!-- end id572 -->
<!-- npu="IPV350" id573 -->
- IPV350：不支持
<!-- end id573 -->
- MC62CM12A AI处理器：不支持

## 功能说明

设置字符串类型标量的属性值。

## 函数原型

```c
aclError aclopSetAttrString(aclopAttr *attr, const char *attrName, const char *attrValue)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr | 输出 | aclopAttr类型的指针。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| attrName | 输入 | 属性名的指针。 |
| attrValue | 输入 | 属性值的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
