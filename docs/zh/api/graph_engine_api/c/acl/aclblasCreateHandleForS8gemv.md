# aclblasCreateHandleForS8gemv

## 产品支持情况

<!-- npu="950" id783 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id783 -->
<!-- npu="A3" id784 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id784 -->
<!-- npu="910b" id785 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id785 -->
<!-- npu="310b" id786 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id786 -->
<!-- npu="310p" id787 -->
- Atlas 推理系列产品：支持
<!-- end id787 -->
<!-- npu="910" id788 -->
- Atlas 训练系列产品：支持
<!-- end id788 -->
<!-- npu="IPV350" id789 -->
- IPV350：不支持
<!-- end id789 -->

## 功能说明

创建矩阵-向量乘的handle，输入数据的数据类型为int8\_t，输出数据的数据类型为int32\_t。创建handle成功后，需调用[aclopExecWithHandle](aclopExecWithHandle.md)接口执行算子。

## 函数原型

```c
aclError aclblasCreateHandleForS8gemv(aclTransType transA,
int m,
int n,
aclComputeType type,
aclopHandle **handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| transA | 输入 | A矩阵是否转置的标记。类型定义请参见[aclTransType](aclTransType.md)。 |
| m | 输入 | 矩阵A的行数，存储矩阵乘数据时，行优先。 |
| n | 输入 | 矩阵A的列数。 |
| type | 输入 | 计算精度，默认高精度。类型定义请参见[aclComputeType](aclComputeType.md)。 |
| handle | 输出 | “算子执行handle的指针”的指针。类型定义请参见[aclopHandle](aclopHandle.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
