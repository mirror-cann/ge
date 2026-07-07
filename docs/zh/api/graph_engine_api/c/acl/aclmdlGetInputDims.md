# aclmdlGetInputDims

## 产品支持情况

<!-- npu="950" id615 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id615 -->
<!-- npu="A3" id616 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id616 -->
<!-- npu="910b" id617 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id617 -->
<!-- npu="310b" id618 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id618 -->
<!-- npu="310p" id619 -->
- Atlas 推理系列产品：支持
<!-- end id619 -->
<!-- npu="910" id620 -->
- Atlas 训练系列产品：支持
<!-- end id620 -->
<!-- npu="IPV350" id621 -->
- IPV350：支持
<!-- end id621 -->

## 功能说明

根据模型描述信息获取模型的输入tensor的维度信息。

如果模型中含有静态AIPP配置信息，您可以根据实际需要选择[aclmdlGetInputDims](aclmdlGetInputDims.md)接口或[aclmdlGetInputDimsV2](aclmdlGetInputDimsV2.md)接口查询维度信息，两者的区别在于：

- 通过[aclmdlGetInputDims](aclmdlGetInputDims.md)接口获取的维度信息，各维度的值与输入图像的各维度的值保持一致。
- 通过[aclmdlGetInputDimsV2](aclmdlGetInputDimsV2.md)接口获取的维度信息，H维度的值 = 输入图像的H维度值\*系数，不同图片格式下C维度的值不同，且各维度值相乘的结果值与通过[aclmdlGetInputSizeByIndex](aclmdlGetInputSizeByIndex.md)接口获取的值保持一致。

## 函数原型

```c
aclError aclmdlGetInputDims(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输入的Dims，index值从0开始。 |
| dims | 输出 | 输入维度信息的指针。类型定义请参见[aclmdlIODims](aclmdlIODims.md)。<br>针对动态Batch、动态分辨率（宽高）的场景，输入tensor的dims中batch size或宽高为-1，表示其动态可变。例如，输入tensor的format为NCHW，在动态Batch场景下，动态可变的输入tensor的dims为[-1,3,224,224]；在动态分辨率场景下，动态可变的输入tensor的dims为[1,3,-1,-1]。举例中的斜体部分以实际情况为准。<br>若tensor的name长度大于127，则在输出的dims.name时，接口会将tensor的name转换为“acl_modelId_${id}_input_${index}_${随机字符串}  ”格式（如果转换后的tensor的name与模型中已有的tensor的name冲突，则会在转换后的name尾部增加“_${随机字符串} ”，否则不会增加随机字符串），并在转换后的name与原name之间建立映射关系，用户可调用[aclmdlGetTensorRealName](aclmdlGetTensorRealName.md)接口，传入转换后的name，获取原name（若向接口传入原name，则获取的还是原name）；若tensor的name长度小于或等于127，则在输出的dims.name时，按tensor的name输出。<br>针对静态AIPP场景，本接口针对不同格式的图像，对应NHWC的Format格式，当前接口中明确各个维度的定义规则，如下表所示。 |

**表 1**  静态AIPP场景下的维度定义规则

| 图像格式 | Format参考格式 | 维度定义规则 |
| --- | --- | --- |
| YUV420SP_U8 | NHWC | n,h,w,c |
| XRGB8888_U8 | NHWC | n,h,w,c |
| RGB888_U8 | NHWC | n,h,w,c |
| YUV400_U8 | NHWC | n,h,w,c |
| ARGB8888_U8 | NHWC | n,h,w,c |
| YUYV_U8 | NHWC | n,h,w,c |
| YUV422SP_U8 | NHWC | n,h,w,c |
| AYUV444_U8 | NHWC | n,h,w,c |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
