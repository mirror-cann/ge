# aclmdlGetCurOutputDims

## 产品支持情况

<!-- npu="950" id727 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id727 -->
<!-- npu="A3" id728 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id728 -->
<!-- npu="910b" id729 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id729 -->
<!-- npu="310b" id730 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id730 -->
<!-- npu="310p" id731 -->
- Atlas 推理系列产品：支持
<!-- end id731 -->
<!-- npu="910" id732 -->
- Atlas 训练系列产品：支持
<!-- end id732 -->
<!-- npu="IPV350" id733 -->
- IPV350：不支持
<!-- end id733 -->

## 功能说明

根据模型描述信息获取指定的模型输出tensor的实际维度信息。

## 函数原型

```c
aclError aclmdlGetCurOutputDims(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输出的Dims，index值从0开始。 |
| dims | 输出 | 输出实际维度信息的指针。类型定义请参见[aclmdlIODims](aclmdlIODims.md)。<br>若tensor的name长度大于127，则在输出的dims.name时，系统会将tensor的name转换为“acl_modelId_\${id}_output_\${index} _\${随机字符串}  ”格式（如果转换后的tensor name与模型中已有的tensor name冲突，则会在转换后的name尾部增加“_${随机字符串} ”，否则不会增加随机字符串），并在转换后的name与原name之间建立映射关系，用户可调用[aclmdlGetTensorRealName](aclmdlGetTensorRealName.md)接口，传入转换后的name，获取原name（若向接口传入原name，则获取的还是原name）；若tensor的name长度小于或等于127，则在输出的dims.name时，按tensor的name输出。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

当前仅支持通过本接口获取以下场景中的模型输出tensor的维度信息：

- 通过模型转换设置多档Batch size或分辨率或维度值，实现动态Batch或动态分辨率或动态维度（ND格式）时，如果用户已调用[aclmdlSetDynamicBatchSize](aclmdlSetDynamicBatchSize.md)设置Batch、或调用[aclmdlSetDynamicHWSize](aclmdlSetDynamicHWSize.md)接口设置输入图片的宽高、或调用[aclmdlSetInputDynamicDims](aclmdlSetInputDynamicDims.md)接口设置某动态维度的值，则可通过该接口获取指定模型输出tensor的实际维度信息；如果用户未调用[aclmdlSetDynamicBatchSize](aclmdlSetDynamicBatchSize.md)接口、或[aclmdlSetDynamicHWSize](aclmdlSetDynamicHWSize.md)接口、或[aclmdlSetInputDynamicDims](aclmdlSetInputDynamicDims.md)接口，则通过该接口可获取最大档的维度信息。
- 固定Shape场景下，通过该接口获取指定的模型输出tensor的维度信息。
