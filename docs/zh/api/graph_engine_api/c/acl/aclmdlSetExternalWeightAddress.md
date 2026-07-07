# aclmdlSetExternalWeightAddress

## 产品支持情况

<!-- npu="950" id57 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id57 -->
<!-- npu="A3" id58 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id58 -->
<!-- npu="910b" id59 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id59 -->
<!-- npu="310b" id60 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id60 -->
<!-- npu="310p" id61 -->
- Atlas 推理系列产品：支持
<!-- end id61 -->
<!-- npu="910" id62 -->
- Atlas 训练系列产品：支持
<!-- end id62 -->
<!-- npu="IPV350" id63 -->
- IPV350：不支持
<!-- end id63 -->

## 功能说明

配置存放外置权重的Device内存，调用方需在模型加载前将外置权重数据复制到该内存中。若存在多个外置权重文件，则需多次调用此接口。

模型加载过程中，计算图中的每一个FileConstant节点会保存对应外置权重文件的路径（例如/xx/../weight\_hasid1），图引擎（ Graph Engine ，简称GE）根据路径中的文件名（例如weight\_hasid1）检索用户是否配置了外置权重的Device内存。若已配置，则直接使用该Device内存地址；否则，GE将自行申请一块Device内存，并读取外置权重文件，并将外置权重数据拷贝至该Device内存中，模型卸载时会释放该内存。

![](figures/Device-Context-Stream之间的关系.png)

**本接口需要与以下其它接口配合**，实现模型加载功能：

1. 调用[aclmdlCreateConfigHandle](aclmdlCreateConfigHandle.md)接口创建模型加载的配置对象。
2. （可选）调用[aclmdlSetExternalWeightAddress](aclmdlSetExternalWeightAddress.md)接口配置存放外置权重的Device内存。
3. 多次调用[aclmdlSetConfigOpt](aclmdlSetConfigOpt.md)接口设置配置对象中每个属性的值。
4. 调用[aclmdlLoadWithConfig](aclmdlLoadWithConfig.md)接口指定模型加载时需要的配置信息，并进行模型加载。
5. 模型加载成功后，调用[aclmdlDestroyConfigHandle](aclmdlDestroyConfigHandle.md)接口销毁。

## 函数原型

```c
aclError aclmdlSetExternalWeightAddress(aclmdlConfigHandle *handle, const char *weightFileName, void *devPtr, size_t size)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| handle | 输出 | 模型加载的配置对象的指针。类型定义请参见[aclmdlConfigHandle](aclmdlConfigHandle.md)。<br>需提前调用[aclmdlCreateConfigHandle](aclmdlCreateConfigHandle.md)接口创建该对象。 |
| weightFileName | 输入 | 外置权重文件名，不含路径。<br>一般对om模型文件大小有限制或模型文件加密的场景下，需单独加载权重文件，因此需在构建模型时，将权重保存在单独的文件中。例如在使用ATC工具生成om文件时，将--external_weight参数设置为1（1表示将原始网络中的Const/Constant节点的权重保存在单独的文件中）。 |
| devPtr | 输入 | Device上存放外置权重的内存地址指针。<br>该Device内存由用户自行管理，模型执行过程中不能释放该内存。 |
| size | 输入 | 内存的大小，单位Byte，需32字节对齐。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
