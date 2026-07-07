# aclmdlGetOutputDims

## 产品支持情况

<!-- npu="950" id546 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id546 -->
<!-- npu="A3" id547 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id547 -->
<!-- npu="910b" id548 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id548 -->
<!-- npu="310b" id549 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id549 -->
<!-- npu="310p" id550 -->
- Atlas 推理系列产品：支持
<!-- end id550 -->
<!-- npu="910" id551 -->
- Atlas 训练系列产品：支持
<!-- end id551 -->
<!-- npu="IPV350" id552 -->
- IPV350：支持
<!-- end id552 -->

## 功能说明

根据模型描述信息获取指定的模型输出tensor的维度信息。

固定Shape场景下，通过该接口获取指定的模型输出tensor的维度信息。

动态Shape（动态Batch或动态分辨率）场景下，通过该接口获取最大档的维度信息。

## 函数原型

```c
aclError aclmdlGetOutputDims(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输出的Dims，index值从0开始。 |
| dims | 输出 | 输出维度信息的指针。类型定义请参见[aclmdlIODims](aclmdlIODims.md)。<br>若tensor的name长度大于127，则在输出的dims.name时，系统会将tensor的name转换为“acl_modelId_${id}_output_${index} _${随机字符串}  ”格式（如果转换后的tensor name与模型中已有的tensor name冲突，则会在转换后的name尾部增加“_${随机字符串} ”，否则不会增加随机字符串），并在转换后的name与原name之间建立映射关系，用户可调用[aclmdlGetTensorRealName](aclmdlGetTensorRealName.md)接口，传入转换后的name，获取原name（若向接口传入原name，则获取的还是原name）；若tensor的name长度小于或等于127，则在输出的dims.name时，按tensor的name输出。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
