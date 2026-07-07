# aclmdlGetInputDataType

## 产品支持情况

<!-- npu="950" id923 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id923 -->
<!-- npu="A3" id924 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id924 -->
<!-- npu="910b" id925 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id925 -->
<!-- npu="310b" id926 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id926 -->
<!-- npu="310p" id927 -->
- Atlas 推理系列产品：支持
<!-- end id927 -->
<!-- npu="910" id928 -->
- Atlas 训练系列产品：支持
<!-- end id928 -->
<!-- npu="IPV350" id929 -->
- IPV350：支持
<!-- end id929 -->

## 功能说明

根据模型描述信息获取模型中指定输入的数据类型。

## 函数原型

```c
aclDataType aclmdlGetInputDataType(const aclmdlDesc *modelDesc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输入的数据类型，index值从0开始。 |

## 返回值说明

返回指定输入的数据类型，类型为aclDataType。
