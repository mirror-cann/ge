# aclmdlGetOutputSizeByIndex

## 产品支持情况

<!-- npu="950" id141 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id141 -->
<!-- npu="A3" id142 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id142 -->
<!-- npu="910b" id143 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id143 -->
<!-- npu="310b" id144 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id144 -->
<!-- npu="310p" id145 -->
- Atlas 推理系列产品：支持
<!-- end id145 -->
<!-- npu="910" id146 -->
- Atlas 训练系列产品：支持
<!-- end id146 -->
<!-- npu="IPV350" id147 -->
- IPV350：支持
<!-- end id147 -->

## 功能说明

根据模型描述信息获取指定输出的大小，单位为Byte。

## 函数原型

```c
size_t aclmdlGetOutputSizeByIndex(aclmdlDesc *modelDesc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输出的大小，index值从0开始。 |

## 返回值说明

针对动态Batch、动态分辨率（宽高）的场景，返回最大档位对应的输出的大小；静态场景下，返回指定输出的大小。单位是Byte。

## 约束说明

如果通过本接口获取的大小为0，有可能是由于输出Shape的范围不确定，当前支持以下两种处理方式：

- 方式一：**系统内部自行申请对应index的输出内存**，节省内存，但内存数据使用结束后，需由用户释放内存，同时，系统内部申请内存时涉及内存拷贝，可能涉及性能损耗。该方式仅支持在使用[aclmdlExecute](aclmdlExecute.md)、[aclmdlExecuteV2](aclmdlExecuteV2.md)推理接口时使用。

    调用aclCreateDataBuffer接口创建存放对应index**输出数据**的aclDataBuffer类型时，**支持在data参数处传入nullptr，同时size需设置为0**，表示创建一个空的aclDataBuffer类型，然后在模型执行过程中，系统**内部自行计算并申请**该index输出的内存。使用该方式可节省内存，但内存数据使用结束后，需由用户释放内存并重置aclDataBuffer，同时，系统内部申请内存时涉及内存拷贝，可能涉及性能损耗。

    释放内存并重置aclDataBuffer的示例代码如下：

    ```c++
    aclDataBuffer *dataBuffer = aclmdlGetDatasetBuffer(output, 0); // 根据index获取对应的dataBuffer
    void *data = aclGetDataBufferAddr(dataBuffer);  // 获取data的Device指针
    aclrtFree(data ); // 释放Device内存
    aclUpdateDataBuffer(dataBuffer, nullptr, 0); // 重置dataBuffer里面内容，以便下次推理
    ```

- 方式二：**用户预估输出内存大小，并申请内存**，由用户自行管理内存，但内存大小可能不够或超出，不够时系统会校验报错，超出时会浪费内存。

    用户需先根据实际情况预估一块较大的输出内存，在模型执行过程中，系统会校验用户指定的输出内存大小是否符合要求，如果不符合要求，系统会返回报错，并在报错信息中提示具体需要多大的输出内存。您可以通过以下两种方式查看报错：

    - 获取应用类日志文件，查看ERROR级别的报错。
    - 在应用程序中调用aclGetRecentErrMsg接口获取报错。
