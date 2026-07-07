# aclblasHgemm

## 产品支持情况

<!-- npu="950" id1147 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id1147 -->
<!-- npu="A3" id1148 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1148 -->
<!-- npu="910b" id1149 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1149 -->
<!-- npu="310b" id1150 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1150 -->
<!-- npu="310p" id1151 -->
- Atlas 推理系列产品：支持
<!-- end id1151 -->
<!-- npu="910" id1152 -->
- Atlas 训练系列产品：支持
<!-- end id1152 -->
<!-- npu="IPV350" id1153 -->
- IPV350：不支持
<!-- end id1153 -->

## 功能说明

执行矩阵-矩阵的乘法，C = αAB + βC，输入数据和输出数据的数据类型为aclFloat16。异步接口。

## 函数原型

```c
aclError aclblasHgemm(aclTransType transA,
aclTransType transB,
aclTransType transC,
int m,
int n,
int k,
const aclFloat16 *alpha,
const aclFloat16 *matrixA,
int lda,
const aclFloat16 *matrixB,
int ldb,
const aclFloat16 *beta,
aclFloat16 *matrixC,
int ldc,
aclComputeType type,
aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| transA | 输入 | A矩阵是否转置的标记。类型定义请参见[aclTransType](aclTransType.md)。 |
| transB | 输入 | B矩阵是否转置的标记。类型定义请参见[aclTransType](aclTransType.md)。 |
| transC | 输入 | C矩阵的标记。类型定义请参见[aclTransType](aclTransType.md)。<br>当前仅支持ACL_TRANS_N。 |
| m | 输入 | 矩阵A的行数与矩阵C的行数。 |
| n | 输入 | 矩阵B的列数与矩阵C的列数。 |
| k | 输入 | 矩阵A的列数与矩阵B的行数。 |
| alpha | 输入 | 用于执行乘操作的标量α的指针。类型定义请参见aclFloat16。 |
| matrixA | 输入 | 矩阵A的指针。类型定义请参见aclFloat16。 |
| lda | 输入 | A矩阵的主维，此时选择转置，按行优先，则lda为A的列数。预留参数，当前只能设置为-1。 |
| matrixB | 输入 | 矩阵B的指针。类型定义请参见aclFloat16。 |
| ldb | 输入 | B矩阵的主维，此时选择转置，按行优先，则ldb为B的列数。<br>预留参数，当前只能设置为-1。 |
| beta | 输入 | 用于执行乘操作的标量β的指针。类型定义请参见aclFloat16。 |
| matrixC | 输入&输出 | 矩阵C的指针。类型定义请参见aclFloat16。 |
| ldc | 输入 | C矩阵的主维，预留参数，当前只能设置为-1。 |
| type | 输入 | 计算精度，默认高精度。类型定义请参见[aclComputeType](aclComputeType.md)。 |
| stream | 输入 | 执行算子所在的Stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
