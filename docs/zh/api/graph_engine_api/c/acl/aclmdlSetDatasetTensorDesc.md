# aclmdlSetDatasetTensorDesc

## 产品支持情况

<!-- npu="950" id379 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id379 -->
<!-- npu="A3" id380 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id380 -->
<!-- npu="910b" id381 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id381 -->
<!-- npu="310b" id382 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id382 -->
<!-- npu="310p" id383 -->
- Atlas 推理系列产品：支持
<!-- end id383 -->
<!-- npu="910" id384 -->
- Atlas 训练系列产品：支持
<!-- end id384 -->
<!-- npu="IPV350" id385 -->
- IPV350：不支持
<!-- end id385 -->

## 功能说明

如果模型输入或输出的Shape是动态的，在模型执行之前调用本接口设置模型输入或输出的tensor描述信息。

## 函数原型

```c
aclError aclmdlSetDatasetTensorDesc(aclmdlDataset *dataset, aclTensorDesc *tensorDesc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| dataset | 输出 | 待增加aclTensorDesc的aclmdlDataset地址指针，表示模型执行的输入或输出数据结构。<br>需提前调用[aclmdlCreateDataset](aclmdlCreateDataset.md)接口创建aclmdlDataset类型的数据，再调用[aclmdlAddDatasetBuffer](aclmdlAddDatasetBuffer.md)接口向aclmdlDataset中增加aclDataBuffer。 |
| tensorDesc | 输入 | 待增加的aclTensorDesc地址指针，表示模型执行时对应的输入或输出的tensor描述。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型的数据，当前只有设置模型输入、输出tensor描述信息中的维度信息有效（对应[aclCreateTensorDesc](aclCreateTensorDesc.md)接口中的代表维度个数的numDims参数、代表维度大小的dims参数），设置数据类型、Format无效。<br>此处设置的维度个数、维度大小必须在模型编译时设置的输入Shape范围内。 |
| index | 输入 | 表示第几个输入或输出的序号。<br>模型存在多个输入、输出时，为避免序号出错，可以先调用[aclmdlGetInputNameByIndex](aclmdlGetInputNameByIndex.md)、[aclmdlGetOutputNameByIndex](aclmdlGetOutputNameByIndex.md)接口获取输入、输出的名称，根据输入、输出名称所对应的index来设置。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

对同一个模型，[aclmdlSetDynamicBatchSize](aclmdlSetDynamicBatchSize.md)接口、[aclmdlSetDynamicHWSize](aclmdlSetDynamicHWSize.md)接口、[aclmdlSetInputDynamicDims](aclmdlSetInputDynamicDims.md)接口、aclmdlSetDatasetTensorDesc接口，只能调用其中一个接口。
