# aclblasCreateHandleForHgemm

## 产品支持情况

<!-- npu="950" id484 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id484 -->
<!-- npu="A3" id485 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id485 -->
<!-- npu="910b" id486 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id486 -->
<!-- npu="310b" id487 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id487 -->
<!-- npu="310p" id488 -->
- Atlas 推理系列产品：支持
<!-- end id488 -->
<!-- npu="910" id489 -->
- Atlas 训练系列产品：支持
<!-- end id489 -->
<!-- npu="IPV350" id490 -->
- IPV350：不支持
<!-- end id490 -->

## 功能说明

创建矩阵-矩阵乘的handle，输入数据和输出数据的数据类型为aclFloat16。创建handle成功后，需调用[aclopExecWithHandle](aclopExecWithHandle.md)接口执行算子。

## 函数原型

```c
aclError aclblasCreateHandleForHgemm(aclTransType transA,
aclTransType transB,
aclTransType transC,
int m,
int n,
int k,
aclComputeType type,
aclopHandle **handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| transA | 输入 | A矩阵是否转置的标记。类型定义请参见[aclTransType](aclTransType.md)。 |
| transB | 输入 | B矩阵是否转置的标记。类型定义请参见[aclTransType](aclTransType.md)。 |
| transC | 输入 | C矩阵的标记，当前仅支持ACL_TRANS_N。类型定义请参见[aclTransType](aclTransType.md)。 |
| m | 输入 | 矩阵A的行数与矩阵C的行数。 |
| n | 输入 | 矩阵B的列数与矩阵C的列数。 |
| k | 输入 | 矩阵A的列数与矩阵B的行数。 |
| type | 输入 | 计算精度，默认高精度。类型定义请参见[aclComputeType](aclComputeType.md)。 |
| handle | 输出 | “算子执行handle的指针”的指针。类型定义请参见[aclopHandle](aclopHandle.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
