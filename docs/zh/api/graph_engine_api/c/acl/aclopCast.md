# aclopCast

## 产品支持情况

<!-- npu="950" id650 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id650 -->
<!-- npu="A3" id651 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id651 -->
<!-- npu="910b" id652 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id652 -->
<!-- npu="310b" id653 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id653 -->
<!-- npu="310p" id654 -->
- Atlas 推理系列产品：支持
<!-- end id654 -->
<!-- npu="910" id655 -->
- Atlas 训练系列产品：支持
<!-- end id655 -->
<!-- npu="IPV350" id656 -->
- IPV350：不支持
<!-- end id656 -->

## 功能说明

转换输入数据的数据类型。异步接口。

## 函数原型

```c
aclError aclopCast(const aclTensorDesc *srcDesc,
const aclDataBuffer *srcBuffer,
const aclTensorDesc *dstDesc,
aclDataBuffer *dstBuffer,
uint8_t truncate,
aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| srcDesc | 输入 | 输入tensor描述的指针。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。 |
| srcBuffer | 输入 | 输入tensor的指针。类型定义请参见aclDataBuffer。 |
| dstDesc | 输入 | 输出tensor描述的指针。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。 |
| dstBuffer | 输出 | 输出tensor的指针。类型定义请参见aclDataBuffer。 |
| truncate | 输入 | 预留。 |
| stream | 输入 | 执行算子所在的Stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
