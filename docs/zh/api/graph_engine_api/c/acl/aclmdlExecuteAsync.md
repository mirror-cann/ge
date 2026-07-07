# aclmdlExecuteAsync

## 产品支持情况

<!-- npu="950" id295 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id295 -->
<!-- npu="A3" id296 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id296 -->
<!-- npu="910b" id297 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id297 -->
<!-- npu="310b" id298 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id298 -->
<!-- npu="310p" id299 -->
- Atlas 推理系列产品：支持
<!-- end id299 -->
<!-- npu="910" id300 -->
- Atlas 训练系列产品：支持
<!-- end id300 -->
<!-- npu="IPV350" id301 -->
- IPV350：不支持
<!-- end id301 -->

## 功能说明

执行模型推理。异步接口。

## 函数原型

```c
aclError aclmdlExecuteAsync(uint32_t modelId, const aclmdlDataset *input, aclmdlDataset *output, aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 指定需要执行推理的模型的ID。<br>调用模型加载接口（例如[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口、[aclmdlLoadFromMem](aclmdlLoadFromMem.md)等）成功后，会返回模型ID，该ID作为本接口的输入。 |
| input | 输入 | 模型推理的输入数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。 |
| output | 输出 | 模型推理的输出数据的指针。类型定义请参见[aclmdlDataset](aclmdlDataset.md)。 |
| stream | 输入 | 指定Stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

- 对同一个modelId的模型，不能调用aclmdlExecuteAsync接口执行多Stream并发场景下的模型推理。错误示例如下，该示例中，两次aclmdlExecuteAsync接口多Stream并发执行，导致报错：

    ```c++
    //......
    aclmdlExecuteAsync(modelId1, input, output, stream1);
    aclmdlExecuteAsync(modelId1, input, output, stream2);
    aclrtSynchronizeStream(stream1);
    aclrtSynchronizeStream(stream2);
    //......
    ```

- 若由于业务需求，必须在多线程中使用同一个modelId，则用户线程间需加锁，保证刷新输入输出内存、保证执行是连续操作，例如：

    ```c++
    // 线程A的接口调用顺序：
    lock(handle1) -> aclrtMemcpyAsync(stream1)刷新输入输出内存 -> aclmdlExecuteAsync(modelId1,stream1)执行推理 -> unlock(handle1)

    // 线程B的接口调用顺序：
    lock(handle1) -> aclrtMemcpyAsync(stream1)刷新输入输出内存 -> aclmdlExecuteAsync(modelId1,stream1)执行推理 -> unlock(handle1)
    ```

- 若需要使用外置Allocator，则注册Allocator时的stream需与模型执行时的stream保持一致。
- 存放模型输入/输出数据的内存为Device内存，可以调用aclrtMalloc、hi\_mpi\_dvpp\_malloc等接口申请Device内存。hi\_mpi\_dvpp\_malloc接口是媒体数据处理功能专用的内存申请接口，一般从性能角度，为了减少拷贝，媒体数据处理的输出作为模型推理的输入，实现内存复用。但由于媒体数据处理访问的地址空间有限，为确保媒体数据处理时内存足够，除媒体数据处理功能外的其它功能（例如，模型加载），建议调用其它接口申请内存，例如aclrtMalloc接口。
