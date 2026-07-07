# aclmdlSetDynamicHWSize

## 产品支持情况

<!-- npu="950" id169 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id169 -->
<!-- npu="A3" id170 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id170 -->
<!-- npu="910b" id171 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id171 -->
<!-- npu="310b" id172 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id172 -->
<!-- npu="310p" id173 -->
- Atlas 推理系列产品：支持
<!-- end id173 -->
<!-- npu="910" id174 -->
- Atlas 训练系列产品：支持
<!-- end id174 -->
<!-- npu="IPV350" id175 -->
- IPV350：不支持
<!-- end id175 -->

## 功能说明

动态分辨率场景下，在模型执行前调用本接口设置模型推理时输入图片的高和宽。

## 函数原型

```c
aclError aclmdlSetDynamicHWSize(uint32_t modelId, aclmdlDataset *dataset, size_t index, uint64_t height, uint64_t width)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 模型ID。<br>调用[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口/[aclmdlLoadFromMem](aclmdlLoadFromMem.md)接口/[aclmdlLoadFromFileWithMem](aclmdlLoadFromFileWithMem.md)接口/[aclmdlLoadFromMemWithMem](aclmdlLoadFromMemWithMem.md)接口加载模型成功后，会返回模型ID。 |
| dataset | 输入&输出 | 模型推理的输入数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。<br>使用aclmdlDataset类型的数据描述模型推理时的输入数据，输入的内存地址、内存大小用aclDataBuffer类型的数据来描述。 |
| index | 输入 | 标识动态分辨率输入的输入index，需调用[aclmdlGetInputIndexByName](aclmdlGetInputIndexByName.md)接口获取，输入名称固定为ACL_DYNAMIC_TENSOR_NAME。 |
| height | 输入 | 需设置的H值。<br>此处设置的分辨率只能是模型编译时设置的分辨率档位中的某一档。<br>如果不清楚模型编译时的分辨率档位，也可以调用[aclmdlGetDynamicHW](aclmdlGetDynamicHW.md)接口获取指定模型支持的分辨率档位数以及每一档中的宽、高。 |
| width | 输入 | 需设置的W值。<br>此处设置的分辨率只能是模型编译时设置的分辨率档位中的某一档。<br>如果不清楚模型编译时的分辨率档位，也可以调用[aclmdlGetDynamicHW](aclmdlGetDynamicHW.md)接口获取指定模型支持的分辨率档位数以及每一档中的宽、高。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
