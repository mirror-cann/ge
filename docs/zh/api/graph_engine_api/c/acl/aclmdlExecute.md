# aclmdlExecute

## 产品支持情况

<!-- npu="950" id755 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id755 -->
<!-- npu="A3" id756 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id756 -->
<!-- npu="910b" id757 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id757 -->
<!-- npu="310b" id758 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id758 -->
<!-- npu="310p" id759 -->
- Atlas 推理系列产品：支持
<!-- end id759 -->
<!-- npu="910" id760 -->
- Atlas 训练系列产品：支持
<!-- end id760 -->
<!-- npu="IPV350" id761 -->
- IPV350：不支持
<!-- end id761 -->

## 功能说明

执行模型推理，直到返回推理结果。

模型加载、模型执行、模型卸载的操作必须在同一个Context下（关于Context的创建请参见aclrtCreateContext）。

## 函数原型

```c
aclError aclmdlExecute(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 指定推理模型的ID。<br>调用模型加载接口（例如[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口、[aclmdlLoadFromMem](aclmdlLoadFromMem.md)等）成功后，会返回模型ID，该ID作为本接口的输入。 |
| input | 输入 | 模型推理的输入数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。 |
| output | 输出 | 模型推理输出数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。<br>调用aclCreateDataBuffer接口创建存放对应index输出数据的aclDataBuffer类型时，支持在data参数处传入nullptr，同时size需设置为0，表示创建一个空的aclDataBuffer类型，然后在模型执行过程中，系统内部自行计算并申请该index输出的内存。使用该方式可节省内存，但内存数据使用结束后，需由用户释放内存并重置aclDataBuffer，同时，系统内部申请内存时涉及内存拷贝，可能涉及性能损耗。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

- 若由于业务需求，必须在多线程中使用同一个modelId，则用户线程间需加锁，保证刷新输入输出内存、保证执行是连续操作，例如：

    ```c++
    // 线程A的接口调用顺序：
    lock(handle1) -> aclrtMemcpy刷新输入输出内存 -> aclmdlExecute执行推理 -> unlock(handle1)

    // 线程B的接口调用顺序：
    lock(handle1) -> aclrtMemcpy刷新输入输出内存 -> aclmdlExecute执行推理 -> unlock(handle1)
    ```

- 存放模型输入/输出数据的内存为Device内存，可以调用aclrtMalloc、hi\_mpi\_dvpp\_malloc等接口申请Device内存。hi\_mpi\_dvpp\_malloc接口是媒体数据处理功能专用的内存申请接口，一般从性能角度，为了减少拷贝，媒体数据处理的输出作为模型推理的输入，实现内存复用。但由于媒体数据处理访问的地址空间有限，为确保媒体数据处理时内存足够，除媒体数据处理功能外的其它功能（例如，模型加载），建议调用其它接口申请内存，例如aclrtMalloc接口。

## 示例代码

释放内存并重置aclDataBuffer的示例代码如下：

```c
aclDataBuffer *dataBuffer = aclmdlGetDatasetBuffer(output, 0); // 根据index获取对应的dataBuffer
void *data = aclGetDataBufferAddr(dataBuffer);  // 获取data的Device指针
aclrtFree(data ); // 释放Device内存
aclUpdateDataBuffer(dataBuffer, nullptr, 0); // 重置dataBuffer里面内容，以便下次推理
```
