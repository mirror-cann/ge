# aclmdlGetDescFromMem

## 产品支持情况

<!-- npu="950" id407 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id407 -->
<!-- npu="A3" id408 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id408 -->
<!-- npu="910b" id409 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id409 -->
<!-- npu="310b" id410 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id410 -->
<!-- npu="310p" id411 -->
- Atlas 推理系列产品：支持
<!-- end id411 -->
<!-- npu="910" id412 -->
- Atlas 训练系列产品：支持
<!-- end id412 -->
<!-- npu="IPV350" id413 -->
- IPV350：支持
<!-- end id413 -->

## 功能说明

从内存获取该模型的模型描述信息。

通过本接口获取到的模型描述信息，无法应用于[aclmdlGetOpAttr](aclmdlGetOpAttr.md)、[aclmdlGetCurOutputDims](aclmdlGetCurOutputDims.md)接口。

## 函数原型

```c
aclError aclmdlGetDescFromMem(aclmdlDesc *modelDesc, const void *model, size_t modelSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输出 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| model | 输入 | 存放模型数据的内存地址指针。 |
| modelSize | 输入 | 内存中的模型数据长度，单位Byte。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
Ascend EP形态下，model处需申请Host上的内存。
<!-- end id1 -->

<!-- npu="310b" id2 -->
Ascend RC形态下，model处需申请Device上的内存。
<!-- end id2 -->

<!-- npu="310p" id3 -->
Control CPU开放形态下，model处需申请Device上的内存。
<!-- end id3 -->

<!-- npu="IPV350" id4 -->
model处需申请Device上的内存。
<!-- end id4 -->
