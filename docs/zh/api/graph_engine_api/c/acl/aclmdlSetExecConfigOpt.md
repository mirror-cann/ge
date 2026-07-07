# aclmdlSetExecConfigOpt

## 产品支持情况

<!-- npu="950" id232 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id232 -->
<!-- npu="A3" id233 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id233 -->
<!-- npu="910b" id234 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id234 -->
<!-- npu="310b" id235 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id235 -->
<!-- npu="310p" id236 -->
- Atlas 推理系列产品：支持
<!-- end id236 -->
<!-- npu="910" id237 -->
- Atlas 训练系列产品：支持
<!-- end id237 -->
<!-- npu="IPV350" id238 -->
- IPV350：支持
<!-- end id238 -->

## 功能说明

设置模型执行的配置对象中的各属性的取值。

本接口需要配合其它接口一起使用，实现模型执行，接口调用顺序如下：

1. 调用[aclmdlCreateExecConfigHandle](aclmdlCreateExecConfigHandle.md)接口创建模型执行的配置对象。
2. 多次调用[aclmdlSetExecConfigOpt](aclmdlSetExecConfigOpt.md)接口设置配置对象中每个属性的值。
3. 调用[aclmdlExecuteV2](aclmdlExecuteV2.md)或[aclmdlExecuteAsyncV2](aclmdlExecuteAsyncV2.md)接口指定模型执行时需要的配置信息，并进行模型执行。
4. 模型执行成功后，调用[aclmdlDestroyExecConfigHandle](aclmdlDestroyExecConfigHandle.md)接口销毁。

## 函数原型

```c
aclError aclmdlSetExecConfigOpt(aclmdlExecConfigHandle *handle, aclmdlExecConfigAttr attr, const void *attrValue, size_t valueSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| handle | 输出 | 模型执行的配置对象的指针。类型定义请参见[aclmdlExecConfigHandle](aclmdlExecConfigHandle.md)。<br>需提前调用[aclmdlCreateExecConfigHandle](aclmdlCreateExecConfigHandle.md)接口创建该对象。 |
| attr | 输入 | 指定需设置的属性。类型定义请参见[aclmdlExecConfigAttr](aclmdlExecConfigAttr.md)。 |
| attrValue | 输入 | 指向属性值的指针，attr对应的属性取值。<br>如果属性值本身是指针，则传入该指针的地址。 |
| valueSize | 输入 | attrValue部分的数据长度。<br>用户可使用C/C++标准库的函数sizeof(*attrValue)查询数据长度。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
