# aclmdlExecuteV2

## 产品支持情况

<!-- npu="950" id1140 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1140 -->
<!-- npu="A3" id1141 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1141 -->
<!-- npu="910b" id1142 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1142 -->
<!-- npu="310b" id1143 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1143 -->
<!-- npu="310p" id1144 -->
- Atlas 推理系列产品：支持
<!-- end id1144 -->
<!-- npu="910" id1145 -->
- Atlas 训练系列产品：支持
<!-- end id1145 -->
<!-- npu="IPV350" id1146 -->
- IPV350：支持
<!-- end id1146 -->

## 功能说明

根据[aclmdlSetExecConfigOpt](aclmdlSetExecConfigOpt.md)接口所配置的属性，执行模型推理，直到返回推理结果。该接口是在[aclmdlExecute](aclmdlExecute.md)接口基础上进行了增强，支持在执行模型推理时控制一些配置参数。

本接口需要配合其它接口一起使用，实现模型执行，接口调用顺序如下：

1. 调用[aclmdlCreateExecConfigHandle](aclmdlCreateExecConfigHandle.md)接口创建模型执行的配置对象。
2. 多次调用[aclmdlSetExecConfigOpt](aclmdlSetExecConfigOpt.md)接口设置配置对象中每个属性的值。
3. 调用aclmdlExecuteV2接口指定模型执行时需要的配置信息，并进行模型执行。
4. 模型执行成功后，调用[aclmdlDestroyExecConfigHandle](aclmdlDestroyExecConfigHandle.md)接口销毁。

## 函数原型

```c
aclError aclmdlExecuteV2(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output, aclrtStream stream, const aclmdlExecConfigHandle *handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 指定需要执行推理的模型的ID。<br>调用模型加载接口（例如[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口、[aclmdlLoadFromMem](aclmdlLoadFromMem.md)等）成功后，会返回模型ID，该ID作为本接口的输入。 |
| input | 输入 | 模型推理的输入数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。 |
| output | 输出 | 模型推理的输出数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。<br>调用aclCreateDataBuffer接口创建存放对应index输出数据的aclDataBuffer类型时，支持在data参数处传入nullptr，同时size需设置为0，表示创建一个空的aclDataBuffer类型，然后在模型执行过程中，系统内部自行计算并申请该index输出的内存。使用该方式可节省内存，但内存数据使用结束后，需由用户释放内存并重置aclDataBuffer，同时，系统内部申请内存时涉及内存拷贝，可能涉及性能损耗。 |
| stream | 输入 | 指定Stream。类型定义请参见aclrtStream。 |
| handle | 输入 | 模型执行的配置对象的指针。类型定义请参见[aclmdlExecConfigHandle](aclmdlExecConfigHandle.md)。<br>与[aclmdlSetExecConfigOpt](aclmdlSetExecConfigOpt.md)中的handle保持一致。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
若此处传NULL，则通过[aclmdlSetExecConfigOpt](aclmdlSetExecConfigOpt.md)接口配置的属性值不生效。
<!-- end id1 -->

<!-- npu="IPV350" id2 -->
不支持传NULL，否则返回报错。
<!-- end id2 -->

其它约束与[aclmdlExecute](aclmdlExecute.md)一致。

## 示例代码

释放内存并重置aclDataBuffer的示例代码如下：

```c
aclDataBuffer *dataBuffer = aclmdlGetDatasetBuffer(output, 0); // 根据index获取对应的dataBuffer
void *data = aclGetDataBufferAddr(dataBuffer);  // 获取data的Device指针
aclrtFree(data ); // 释放Device内存
aclUpdateDataBuffer(dataBuffer, nullptr, 0); // 重置dataBuffer里面内容，以便下次推理
```
