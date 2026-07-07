# aclmdlCreateDataset

## 产品支持情况

<!-- npu="950" id895 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id895 -->
<!-- npu="A3" id896 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id896 -->
<!-- npu="910b" id897 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id897 -->
<!-- npu="310b" id898 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id898 -->
<!-- npu="310p" id899 -->
- Atlas 推理系列产品：支持
<!-- end id899 -->
<!-- npu="910" id900 -->
- Atlas 训练系列产品：支持
<!-- end id900 -->
<!-- npu="IPV350" id901 -->
- IPV350：支持
<!-- end id901 -->

## 功能说明

创建aclmdlDataset类型的数据，该数据类型用于描述模型推理时的输入数据、输出数据，模型可能存在多个输入、多个输出，每个输入/输出的内存地址、内存大小用aclDataBuffer类型的数据来描述。

如需销毁aclmdlDataset类型的数据，请参见[aclmdlDestroyDataset](aclmdlDestroyDataset.md)。

## 函数原型

```c
aclmdlDataset *aclmdlCreateDataset()
```

## 参数说明

无

## 返回值说明

返回aclmdlDataset类型的指针。
