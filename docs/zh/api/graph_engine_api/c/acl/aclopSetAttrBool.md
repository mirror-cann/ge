# aclopSetAttrBool

## 产品支持情况

<!-- npu="950" id85 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id85 -->
<!-- npu="A3" id86 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id86 -->
<!-- npu="910b" id87 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id87 -->
<!-- npu="310b" id88 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id88 -->
<!-- npu="310p" id89 -->
- Atlas 推理系列产品：支持
<!-- end id89 -->
<!-- npu="910" id90 -->
- Atlas 训练系列产品：支持
<!-- end id90 -->
<!-- npu="IPV350" id91 -->
- IPV350：不支持
<!-- end id91 -->
- MC62CM12A AI处理器：不支持

## 功能说明

设置bool类型标量的属性值。

## 函数原型

```c
aclError aclopSetAttrBool(aclopAttr *attr, const char *attrName, uint8_t attrValue)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr | 输出 | aclopAttr类型的指针。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| attrName | 输入 | 属性名的指针。 |
| attrValue | 输入 | 属性值。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
