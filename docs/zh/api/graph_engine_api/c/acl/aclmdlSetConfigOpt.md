# aclmdlSetConfigOpt

## 产品支持情况

<!-- npu="950" id881 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id881 -->
<!-- npu="A3" id882 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id882 -->
<!-- npu="910b" id883 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id883 -->
<!-- npu="310b" id884 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id884 -->
<!-- npu="310p" id885 -->
- Atlas 推理系列产品：支持
<!-- end id885 -->
<!-- npu="910" id886 -->
- Atlas 训练系列产品：支持
<!-- end id886 -->
<!-- npu="IPV350" id887 -->
- IPV350：支持
<!-- end id887 -->

## 功能说明

设置模型加载的配置对象中的各属性的取值，包括模型执行的优先级、模型的文件路径或内存地址、内存大小等。

**本接口需要与以下其它接口配合**，实现模型加载功能：

1. 调用[aclmdlCreateConfigHandle](aclmdlCreateConfigHandle.md)接口创建模型加载的配置对象。
2. （可选）调用[aclmdlSetExternalWeightAddress](aclmdlSetExternalWeightAddress.md)接口配置存放外置权重的Device内存。
3. 多次调用[aclmdlSetConfigOpt](aclmdlSetConfigOpt.md)接口设置配置对象中每个属性的值。
4. 调用[aclmdlLoadWithConfig](aclmdlLoadWithConfig.md)接口指定模型加载时需要的配置信息，并进行模型加载。
5. 模型加载成功后，调用[aclmdlDestroyConfigHandle](aclmdlDestroyConfigHandle.md)接口销毁。

## 函数原型

```c
aclError aclmdlSetConfigOpt(aclmdlConfigHandle *handle, aclmdlConfigAttr attr, const void *attrValue, size_t valueSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| handle | 输出 | 模型加载的配置对象的指针。类型定义请参见[aclmdlConfigHandle](aclmdlConfigHandle.md)。<br>需提前调用[aclmdlCreateConfigHandle](aclmdlCreateConfigHandle.md)接口创建该对象。 |
| attr | 输入 | 指定需设置的属性。类型定义请参见[aclmdlConfigAttr](aclmdlConfigAttr.md)。 |
| attrValue | 输入 | 指向属性值的指针，attr对应的属性取值。<br>如果属性值本身是指针，则传入该指针的地址。 |
| valueSize | 输入 | attrValue部分的数据长度。<br>用户可使用C/C++标准库的函数sizeof(*attrValue)查询数据长度。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 参考资源

使用[aclmdlSetConfigOpt](aclmdlSetConfigOpt.md)接口、[aclmdlLoadWithConfig](aclmdlLoadWithConfig.md)接口时，是通过配置对象中的属性来区分，在加载模型时是从文件加载，还是从内存加载，以及内存是由系统内部管理，还是由用户管理。

当前还提供了以下接口实现模型加载的功能，从使用的接口上区分从文件加载，还是从内存加载，以及内存是由系统内部管理，还是由用户管理。

- [aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口
- [aclmdlLoadFromMem](aclmdlLoadFromMem.md)接口
- [aclmdlLoadFromFileWithMem](aclmdlLoadFromFileWithMem.md)接口
- [aclmdlLoadFromMemWithMem](aclmdlLoadFromMemWithMem.md)接口
- [aclmdlLoadFromFileWithQ](aclmdlLoadFromFileWithQ.md)接口
- [aclmdlLoadFromMemWithQ](aclmdlLoadFromMemWithQ.md)接口
