# aclmdlSetInputDynamicDims

## 产品支持情况

<!-- npu="950" id643 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id643 -->
<!-- npu="A3" id644 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id644 -->
<!-- npu="910b" id645 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id645 -->
<!-- npu="310b" id646 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id646 -->
<!-- npu="310p" id647 -->
- Atlas 推理系列产品：支持
<!-- end id647 -->
<!-- npu="910" id648 -->
- Atlas 训练系列产品：支持
<!-- end id648 -->
<!-- npu="IPV350" id649 -->
- IPV350：不支持
<!-- end id649 -->

## 功能说明

如果模型输入的Shape是动态的、输入数据Format为ND格式（ND表示支持任意格式），在模型执行前调用本接口设置模型推理时具体维度的值。

## 函数原型

```c
aclError aclmdlSetInputDynamicDims(uint32_t modelId, aclmdlDataset *dataset, size_t index, const aclmdlIODims *dims)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 模型ID。<br>调用[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口/[aclmdlLoadFromMem](aclmdlLoadFromMem.md)接口/[aclmdlLoadFromFileWithMem](aclmdlLoadFromFileWithMem.md)接口/[aclmdlLoadFromMemWithMem](aclmdlLoadFromMemWithMem.md)接口加载模型成功后，会返回模型ID。 |
| dataset | 输入&输出 | 模型推理的输入数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。<br>使用aclmdlDataset类型的数据描述模型推理时的输入数据，输入的内存地址、内存大小用aclDataBuffer类型的数据来描述。 |
| index | 输入 | 标识动态维度的输入index。<br>需调用[aclmdlGetInputIndexByName](aclmdlGetInputIndexByName.md)接口获取，输入名称固定为ACL_DYNAMIC_TENSOR_NAME。 |
| dims | 输入 | 具体某一档上的所有维度信息的指针。类型定义请参见[aclmdlIODims](aclmdlIODims.md)。<br>此处设置的动态维度的值只能是模型编译时设置的档位中的某一档。<br>例如：使用ATC工具进行模型转换时，input_shape="data:1,1,40,-1;label:1,-1;mask:-1,-1" ，dynamic_dims="20,20,1,1; 40,40,2,2; 80,60,4,4"，若输入数据的真实维度为（1,1,40,20,1,20,1,1），则dims结构体信息的填充示例如下（name暂不使用）：<br>dims.dimCount = 8<br>dims.dims[0] = 1<br>dims.dims[1] = 1<br>dims.dims[2] = 40<br>dims.dims[3] = 20<br>dims.dims[4] = 1<br>dims.dims[5] = 20<br>dims.dims[6] = 1<br>dims.dims[7] = 1<br>如果不清楚模型编译时的动态维度档位，也可以调用[aclmdlGetInputDynamicDims](aclmdlGetInputDynamicDims.md)接口获取指定模型支持的动态维度档位数以及每一档中的值。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
