# aclopCreateHandleForCast

## 产品支持情况

<!-- npu="950" id574 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id574 -->
<!-- npu="A3" id575 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id575 -->
<!-- npu="910b" id576 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id576 -->
<!-- npu="310b" id577 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id577 -->
<!-- npu="310p" id578 -->
- Atlas 推理系列产品：支持
<!-- end id578 -->
<!-- npu="910" id579 -->
- Atlas 训练系列产品：支持
<!-- end id579 -->
<!-- npu="IPV350" id580 -->
- IPV350：不支持
<!-- end id580 -->

## 功能说明

创建数据类型转换的handle。创建handle成功后，需调用[aclopExecWithHandle](aclopExecWithHandle.md)接口执行算子。

## 函数原型

```c
aclError aclopCreateHandleForCast(aclTensorDesc *srcDesc,
aclTensorDesc *dstDesc,
uint8_t truncate,
aclopHandle **handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| srcDesc | 输入 | 输入tensor描述的指针。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。 |
| dstDesc | 输入 | 输出tensor描述的指针。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。 |
| truncate | 输入 | 预留。 |
| handle | 输出 | “算子执行handle的指针”的指针。类型定义请参见[aclopHandle](aclopHandle.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
