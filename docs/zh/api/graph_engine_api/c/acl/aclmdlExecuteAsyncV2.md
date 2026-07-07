# aclmdlExecuteAsyncV2

## 产品支持情况

<!-- npu="950" id372 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id372 -->
<!-- npu="A3" id373 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id373 -->
<!-- npu="910b" id374 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id374 -->
<!-- npu="310b" id375 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id375 -->
<!-- npu="310p" id376 -->
- Atlas 推理系列产品：不支持
<!-- end id376 -->
<!-- npu="910" id377 -->
- Atlas 训练系列产品：不支持
<!-- end id377 -->
<!-- npu="IPV350" id378 -->
- IPV350：支持
<!-- end id378 -->

## 功能说明

根据[aclmdlSetExecConfigOpt](aclmdlSetExecConfigOpt.md)所配置的属性，执行模型推理，直到返回推理结果。该接口是在[aclmdlExecuteAsync](aclmdlExecuteAsync.md)接口基础上进行了增强，支持在执行模型推理时控制一些配置参数。异步接口。

本接口需要配合其它接口一起使用，实现模型执行，接口调用顺序如下：

1. 调用[aclmdlCreateExecConfigHandle](aclmdlCreateExecConfigHandle.md)接口创建模型执行的配置对象。
2. 多次调用[aclmdlSetExecConfigOpt](aclmdlSetExecConfigOpt.md)接口设置配置对象中每个属性的值。
3. 调用aclmdlExecuteAsyncV2接口指定模型执行时需要的配置信息，并进行模型执行。
4. 模型执行成功后，调用[aclmdlDestroyExecConfigHandle](aclmdlDestroyExecConfigHandle.md)接口销毁。

## 函数原型

```c
aclError aclmdlExecuteAsyncV2(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output, aclrtStream stream, const aclmdlExecConfigHandle *handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 指定需要执行推理的模型的ID。<br>调用[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口/[aclmdlLoadFromMem](aclmdlLoadFromMem.md)接口/[aclmdlLoadFromFileWithMem](aclmdlLoadFromFileWithMem.md)接口/[aclmdlLoadFromMemWithMem](aclmdlLoadFromMemWithMem.md)接口加载模型成功后，会返回模型ID。 |
| input | 输入 | 模型推理的输入数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。 |
| output | 输出 | 模型推理的输出数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。 |
| stream | 输入 | 指定Stream。类型定义请参见aclrtStream。 |
| handle | 输入 | 模型执行的配置对象的指针。类型定义请参见[aclmdlExecConfigHandle](aclmdlExecConfigHandle.md)。<br>与[aclmdlSetExecConfigOpt](aclmdlSetExecConfigOpt.md)中的handle保持一致。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

<!-- npu="IPV350" id1 -->
stream参数不支持传NULL，否则返回报错。
<!-- end id1 -->

其它约束与[aclmdlExecuteAsync](aclmdlExecuteAsync.md)一致。
