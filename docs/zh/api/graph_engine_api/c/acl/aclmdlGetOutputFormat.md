# aclmdlGetOutputFormat

## 产品支持情况

<!-- npu="950" id239 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id239 -->
<!-- npu="A3" id240 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id240 -->
<!-- npu="910b" id241 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id241 -->
<!-- npu="310b" id242 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id242 -->
<!-- npu="310p" id243 -->
- Atlas 推理系列产品：支持
<!-- end id243 -->
<!-- npu="910" id244 -->
- Atlas 训练系列产品：支持
<!-- end id244 -->
<!-- npu="IPV350" id245 -->
- IPV350：支持
<!-- end id245 -->

## 功能说明

根据模型描述信息获取模型中指定输出的Format。

## 函数原型

```c
aclFormat aclmdlGetOutputFormat(const aclmdlDesc *modelDesc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输出的Format，index值从0开始。 |

## 返回值说明

返回指定输出的Format，类型为aclFormat。
