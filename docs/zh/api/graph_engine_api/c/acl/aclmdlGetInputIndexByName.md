# aclmdlGetInputIndexByName

## 产品支持情况

<!-- npu="950" id867 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id867 -->
<!-- npu="A3" id868 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id868 -->
<!-- npu="910b" id869 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id869 -->
<!-- npu="310b" id870 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id870 -->
<!-- npu="310p" id871 -->
- Atlas 推理系列产品：支持
<!-- end id871 -->
<!-- npu="910" id872 -->
- Atlas 训练系列产品：支持
<!-- end id872 -->
<!-- npu="IPV350" id873 -->
- IPV350：支持
<!-- end id873 -->

## 功能说明

根据模型中的输入名称获取对应输入的索引编号。

## 函数原型

```c
aclError aclmdlGetInputIndexByName(const aclmdlDesc *modelDesc, const char *name, size_t *index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| name | 输入 | 输入名称的指针。 |
| index | 输出 | 输入的索引编号的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
