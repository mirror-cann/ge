# aclmdlQuerySize

## 产品支持情况

<!-- npu="950" id337 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id337 -->
<!-- npu="A3" id338 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id338 -->
<!-- npu="910b" id339 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id339 -->
<!-- npu="310b" id340 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id340 -->
<!-- npu="310p" id341 -->
- Atlas 推理系列产品：支持
<!-- end id341 -->
<!-- npu="910" id342 -->
- Atlas 训练系列产品：支持
<!-- end id342 -->
<!-- npu="IPV350" id343 -->
- IPV350：支持
<!-- end id343 -->

## 功能说明

根据模型文件获取模型执行时所需的权值内存大小、工作内存大小。

当由用户管理内存时，为确保内存不浪费，在申请工作内存、权值内存前，需要调用本接口查询模型运行时所需工作内存、权值内存的大小。如果模型输入数据的Shape不确定，则不能调用本命接口查询内存大小，在加载模型时，就无法由用户管理内存，因此需选择由系统管理内存的模型加载接口（例如，[aclmdlLoadFromFile](aclmdlLoadFromFile.md)、[aclmdlLoadFromMem](aclmdlLoadFromMem.md)）。

## 函数原型

```c
aclError aclmdlQuerySize(const char *fileName, size_t *workSize, size_t *weightSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| fileName | 输入 | 模型文件路径的指针，路径中包含文件名。运行程序（APP）的用户需要对该路径有访问权限。<br>此处的模型文件是om模型文件。<br>关于如何获取om文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 > 基础功能参数 > 总体选项 > --mode”。 |
| workSize | 输出 | 模型执行时所需的工作内存大小的指针，单位Byte。<br>此处的内存为Device内存，而且需要用户申请和释放。 |
| weightSize | 输出 | 模型执行时所需权值内存大小的指针，单位Byte。<br>此处的内存为Device内存，而且需要用户申请和释放。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
