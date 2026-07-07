# aclmdlGetOutputIndexByName

## 产品支持情况

<!-- npu="950" id581 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id581 -->
<!-- npu="A3" id582 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id582 -->
<!-- npu="910b" id583 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id583 -->
<!-- npu="310b" id584 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id584 -->
<!-- npu="310p" id585 -->
- Atlas 推理系列产品：支持
<!-- end id585 -->
<!-- npu="910" id586 -->
- Atlas 训练系列产品：支持
<!-- end id586 -->
<!-- npu="IPV350" id587 -->
- IPV350：支持
<!-- end id587 -->

## 功能说明

根据模型中的输出名称获取对应输出的索引编号。

## 函数原型

```c
aclError aclmdlGetOutputIndexByName(const aclmdlDesc *modelDesc, const char *name, size_t *index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| name | 输入 | 输出名称的指针。 |
| index | 输出 | 输出的索引编号的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
