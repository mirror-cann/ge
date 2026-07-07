# aclblasS8gemm

## 产品支持情况

<!-- npu="950" id204 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id204 -->
<!-- npu="A3" id205 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id205 -->
<!-- npu="910b" id206 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id206 -->
<!-- npu="310b" id207 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id207 -->
<!-- npu="310p" id208 -->
- Atlas 推理系列产品：支持
<!-- end id208 -->
<!-- npu="910" id209 -->
- Atlas 训练系列产品：支持
<!-- end id209 -->
<!-- npu="IPV350" id210 -->
- IPV350：不支持
<!-- end id210 -->

## 功能说明

执行矩阵-矩阵的乘法，C = αAB + βC，输入数据的数据类型为int8\_t，输出数据的数据类型为int32\_t。异步接口。

## 函数原型

```c
aclError aclblasS8gemm(aclTransType transA,
aclTransType transB,
aclTransType transC,
int m,
int n,
int k,
const int32_t *alpha,
const int8_t *matrixA,
int lda,
const int8_t *matrixB,
int ldb,
const int32_t *beta,
int32_t *matrixC,
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
| alpha | 输入 | 用于执行乘操作的标量α的指针。 |
| matrixA | 输入 | 矩阵A的指针。 |
| lda | 输入 | A矩阵的主维，此时选择转置，按行优先，则lda为A的列数。预留参数，当前只能设置为-1。 |
| matrixB | 输入 | 矩阵B的指针。 |
| ldb | 输入 | B矩阵的主维，此时选择转置，按行优先，则ldb为B的列数。预留参数，当前只能设置为-1。 |
| beta | 输入 | 用于执行乘操作的标量β的指针。 |
| matrixC | 输入&输出 | 矩阵C的指针。 |
| ldc | 输入 | C矩阵的主维，预留参数，当前只能设置为-1。 |
| type | 输入 | 计算精度，默认高精度。类型定义请参见[aclComputeType](aclComputeType.md)。 |
| stream | 输入 | 执行算子所在的Stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
