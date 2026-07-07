# aclmdlGetInputDimsRange

## 产品支持情况

<!-- npu="950" id330 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id330 -->
<!-- npu="A3" id331 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id331 -->
<!-- npu="910b" id332 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id332 -->
<!-- npu="310b" id333 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id333 -->
<!-- npu="310p" id334 -->
- Atlas 推理系列产品：支持
<!-- end id334 -->
<!-- npu="910" id335 -->
- Atlas 训练系列产品：支持
<!-- end id335 -->
<!-- npu="IPV350" id336 -->
- IPV350：不支持
<!-- end id336 -->

## 功能说明

根据模型描述信息获取模型的输入tensor的维度范围。

## 函数原型

```c
aclError aclmdlGetInputDimsRange(const aclmdlDesc *modelDesc, size_t index, aclmdlIODimsRange *dimsRange)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输入的维度信息，index值从0开始。 |
| dimsRange | 输出 | 输入维度信息的指针。类型定义请参见[aclmdlIODimsRange](aclmdlIODimsRange.md)。<br><br>  - 动态Shape且配置Shape范围的场景下，返回Shape范围，动态维度上下限不相同，静态维度上下限相同。例如，输入tensor的format为NCHW，dims为[1,3,10~224,10~300]，N和C维上是固定值，H和W维上是动态范围，那么通过本接口aclmdlIODimsRange.rangeCount为4，aclmdlIODimsRange.range数组中有4个元素，值分别为：{1,1}、{3,3}、{10,224}、{10,300}。<br>  - 动态Shape且配置分档的场景下，不支持使用本接口，aclmdlIODimsRange.rangeCount返回0。<br>  - 静态Shape场景下，返回Shape范围，但各维度上下限相同。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
