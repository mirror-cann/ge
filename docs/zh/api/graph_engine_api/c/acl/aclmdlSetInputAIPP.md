# aclmdlSetInputAIPP

## 产品支持情况

<!-- npu="950" id1196 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1196 -->
<!-- npu="A3" id1197 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1197 -->
<!-- npu="910b" id1198 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1198 -->
<!-- npu="310b" id1199 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1199 -->
<!-- npu="310p" id1200 -->
- Atlas 推理系列产品：支持
<!-- end id1200 -->
<!-- npu="910" id1201 -->
- Atlas 训练系列产品：支持
<!-- end id1201 -->
<!-- npu="IPV350" id1202 -->
- IPV350：不支持
<!-- end id1202 -->

## 功能说明

动态AIPP场景下，根据指定的动态AIPP输入的输入index，设置模型推理时的AIPP参数值。

动态AIPP支持的几种操作的计算方式及其计算顺序如下：抠图-\>色域转换-\>减均值/归一化-\>padding。

## 函数原型

```c
aclError aclmdlSetInputAIPP(uint32_t modelId, aclmdlDataset *dataset, size_t index, const aclmdlAIPP *aippParmsSet)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 模型的ID。<br>调用[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口/[aclmdlLoadFromMem](aclmdlLoadFromMem.md)接口/[aclmdlLoadFromFileWithMem](aclmdlLoadFromFileWithMem.md)接口/[aclmdlLoadFromMemWithMem](aclmdlLoadFromMemWithMem.md)接口加载模型成功后，会返回模型ID。 |
| dataset | 输入 | 模型推理的输入数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。<br>使用aclmdlDataset类型的数据描述模型推理时的输入数据，输入的内存地址、内存大小用aclDataBuffer类型的数据来描述。 |
| index | 输入 | 标识动态AIPP输入的输入index。<br>多个动态AIPP输入的场景下，用户可调用[aclmdlGetAippType](aclmdlGetAippType.md)接口获取指定模型输入所关联的动态AIPP输入的输入index。<br>为保证向前兼容，如果明确只有一个动态AIPP输入，可调用[aclmdlGetInputIndexByName](aclmdlGetInputIndexByName.md)接口获取，输入名称固定为ACL_DYNAMIC_AIPP_NAME。 |
| aippParmsSet | 输入 | 动态AIPP参数对象的指针。类型定义请参见[aclmdlAIPP](aclmdlAIPP.md)。<br>提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

经过动态AIPP处理后的图像的宽、高必须与原始模型中输入Shape中的宽、高保持一致。

多Batch场景下，根据每个Batch的配置计算出动态AIPP后输出图片的宽、高，经过动态AIPP后每个Batch的输出图片宽、高必须是一致的。计算输出图片宽、高的计算公式如下表所示。

抠图或者缩放或者padding之后，对图片宽、高的校验规则如下，其中，aippOutputW、aippOutputH分别表示AIPP输出图片的宽、高，其它参数是[aclmdlSetAIPPSrcImageSize](aclmdlSetAIPPSrcImageSize.md)、[aclmdlSetAIPPScfParams](aclmdlSetAIPPScfParams.md)、[aclmdlSetAIPPCropParams](aclmdlSetAIPPCropParams.md)、[aclmdlSetAIPPPaddingParams](aclmdlSetAIPPPaddingParams.md)接口的入参。

**表 1**  输出图片宽、高计算公式

| 抠图 | 缩放 | 补边（padding） | 动态AIPP输出图片的宽、高 |
| --- | --- | --- | --- |
| 否 | 否 | 否 | aippOutputW=srcImageSizeW，aippOutputH=srcImageSizeH |
| 是 | 否 | 否 | aippOutputW=cropSizeW，aippOutputH=cropSizeH |
| 是 | 是 | 否 | aippOutputW=scfOutputSizeW，aippOutputH=scfOutputSizeH |
| 是 | 否 | 是 | aippOutputW=cropSizeW + paddingSizeLeft + paddingSizeRight，aippOutputH=cropSizeH + paddingSizeTop + paddingSizeBottom |
| 否 | 否 | 是 | aippOutputW=srcImageSizeW + paddingSizeLeft + paddingSizeRight，aippOutputH=srcImageSizeH + paddingSizeTop + paddingSizeBottom |
| 否 | 是 | 是 | aippOutputW=scfOutputSizeW + paddingSizeLeft + paddingSizeRight，aippOutputH=scfOutputSizeH + paddingSizeTop + paddingSizeBottom |
| 否 | 是 | 否 | aippOutputW=scfOutputSizeW，aippOutputH=scfOutputSizeH |
| 是 | 是 | 是 | aippOutputW=scfOutputSizeW + paddingSizeLeft + paddingSizeRight，aippOutputH=scfOutputSizeH + paddingSizeTop + paddingSizeBottom |
