# aclopSetAttrInt

## 产品支持情况

<!-- npu="950" id846 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id846 -->
<!-- npu="A3" id847 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id847 -->
<!-- npu="910b" id848 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id848 -->
<!-- npu="310b" id849 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id849 -->
<!-- npu="310p" id850 -->
- Atlas 推理系列产品：支持
<!-- end id850 -->
<!-- npu="910" id851 -->
- Atlas 训练系列产品：支持
<!-- end id851 -->
<!-- npu="IPV350" id852 -->
- IPV350：不支持
<!-- end id852 -->
- MC62CM12A AI处理器：不支持

## 功能说明

设置int64\_t类型标量的属性值。

## 函数原型

```c
aclError aclopSetAttrInt(aclopAttr *attr, const char *attrName, int64_t attrValue)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr | 输出 | aclopAttr类型的指针。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| attrName | 输入 | 属性名的指针。 |
| attrValue | 输入 | 属性值。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
