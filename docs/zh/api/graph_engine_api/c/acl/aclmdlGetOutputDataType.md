# aclmdlGetOutputDataType

## 产品支持情况

<!-- npu="950" id608 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id608 -->
<!-- npu="A3" id609 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id609 -->
<!-- npu="910b" id610 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id610 -->
<!-- npu="310b" id611 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id611 -->
<!-- npu="310p" id612 -->
- Atlas 推理系列产品：支持
<!-- end id612 -->
<!-- npu="910" id613 -->
- Atlas 训练系列产品：支持
<!-- end id613 -->
<!-- npu="IPV350" id614 -->
- IPV350：支持
<!-- end id614 -->

## 功能说明

根据模型描述信息获取模型中指定输出的数据类型。

## 函数原型

```c
aclDataType aclmdlGetOutputDataType(const aclmdlDesc *modelDesc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输出的数据类型，index值从0开始。 |

## 返回值说明

返回指定输出的数据类型，类型为aclDataType。
