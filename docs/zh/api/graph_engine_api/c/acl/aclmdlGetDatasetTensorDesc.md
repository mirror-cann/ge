# aclmdlGetDatasetTensorDesc

## 产品支持情况

<!-- npu="950" id1133 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1133 -->
<!-- npu="A3" id1134 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1134 -->
<!-- npu="910b" id1135 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1135 -->
<!-- npu="310b" id1136 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1136 -->
<!-- npu="310p" id1137 -->
- Atlas 推理系列产品：支持
<!-- end id1137 -->
<!-- npu="910" id1138 -->
- Atlas 训练系列产品：支持
<!-- end id1138 -->
<!-- npu="IPV350" id1139 -->
- IPV350：不支持
<!-- end id1139 -->

## 功能说明

如果模型输入或输出的Shape是动态的，模型在模型执行之后可以调用本接口从aclmdlDataset类型的数据（该类型用于描述模型推理时的输入数据、输出数据）中获取指定的输入或者输出的Tensor描述信息。

**典型场景举例**：如果模型输入Shape是动态的，在模型执行之前调[aclmdlSetDatasetTensorDesc](aclmdlSetDatasetTensorDesc.md)设置该输入的tensor描述信息，在模型执行之后，调用aclmdlGetDatasetTensorDesc接口获取模型动态输出的Tensor描述信息。

## 函数原型

```c
aclTensorDesc *aclmdlGetDatasetTensorDesc(const aclmdlDataset *dataset, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| dataset | 输入 | 模型执行的输入或输出数据指针。<br>需提前调用[aclmdlCreateDataset](aclmdlCreateDataset.md)接口创建aclmdlDataset类型的数据。 |
| index | 输入 | 表示第几个输入或输出的序号。<br>模型存在多个输入、输出时，为避免序号出错，可以先调用[aclmdlGetInputNameByIndex](aclmdlGetInputNameByIndex.md)、[aclmdlGetOutputNameByIndex](aclmdlGetOutputNameByIndex.md)接口获取输入、输出的名称，根据输入、输出名称所对应的index来设置。 |

## 返回值说明

返回指定输入或输出的tensor描述信息，类型为[aclTensorDesc](aclTensorDesc.md)。
