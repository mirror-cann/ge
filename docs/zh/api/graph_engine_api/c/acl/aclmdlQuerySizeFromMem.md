# aclmdlQuerySizeFromMem

## 产品支持情况

<!-- npu="950" id741 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id741 -->
<!-- npu="A3" id742 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id742 -->
<!-- npu="910b" id743 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id743 -->
<!-- npu="310b" id744 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id744 -->
<!-- npu="310p" id745 -->
- Atlas 推理系列产品：支持
<!-- end id745 -->
<!-- npu="910" id746 -->
- Atlas 训练系列产品：支持
<!-- end id746 -->
<!-- npu="IPV350" id747 -->
- IPV350：不支持
<!-- end id747 -->

## 功能说明

根据内存中的模型数据获取模型执行时所需的权值内存大小、工作内存大小。

当由用户管理内存时，为确保内存不浪费，在申请工作内存、权值内存前，需要调用本接口查询模型运行时所需工作内存、权值内存的大小。如果模型输入数据的Shape不确定，则不能调用本接口查询内存大小，在加载模型时，就无法由用户管理内存，因此需选择由系统管理内存的模型加载接口（例如，[aclmdlLoadFromFile](aclmdlLoadFromFile.md)、[aclmdlLoadFromMem](aclmdlLoadFromMem.md)）。

## 函数原型

```c
aclError aclmdlQuerySizeFromMem(const void *model, size_t modelSize, size_t *workSize, size_t *weightSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model | 输入 | 模型数据的指针。 |
| modelSize | 输入 | 模型数据长度，单位Byte。 |
| workSize | 输出 | 模型执行时所需的工作内存大小的指针，单位Byte。<br>此处的内存为Device内存，而且需要用户申请和释放。 |
| weightSize | 输出 | 模型执行时所需权值内存大小的指针，单位Byte。<br>此处的内存为Device内存，而且需要用户申请和释放。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
