# aclblasHgemv

## 产品支持情况

<!-- npu="950" id505 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id505 -->
<!-- npu="A3" id506 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id506 -->
<!-- npu="910b" id507 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id507 -->
<!-- npu="310b" id508 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id508 -->
<!-- npu="310p" id509 -->
- Atlas 推理系列产品：支持
<!-- end id509 -->
<!-- npu="910" id510 -->
- Atlas 训练系列产品：支持
<!-- end id510 -->
<!-- npu="IPV350" id511 -->
- IPV350：不支持
<!-- end id511 -->

## 功能说明

执行矩阵-向量的乘法，y = αAx + βy，输入数据和输出数据的数据类型为aclFloat16**。**异步接口。

## 函数原型

```c
aclError aclblasHgemv(aclTransType transA,
int m,
int n,
const aclFloat16 *alpha,
const aclFloat16 *a,
int lda,
const aclFloat16 *x,
int incx,
const aclFloat16 *beta,
aclFloat16 *y,
int incy,
aclComputeType type,
aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| transA | 输入 | A矩阵是否转置的标记。类型定义请参见[aclTransType](aclTransType.md)。 |
| m | 输入 | 矩阵A的行数，存储矩阵乘数据时，行优先。 |
| n | 输入 | 矩阵A的列数。 |
| alpha | 输入 | 用于执行乘操作的标量α的指针。类型定义请参见aclFloat16。 |
| a | 输入 | 矩阵A的指针。类型定义请参见aclFloat16。 |
| lda | 输入 | A矩阵的主维，此时选择转置，按行优先，则lda为A的列数。预留参数，当前只能设置为-1。 |
| x | 输入 | 向量x的指针。类型定义请参见aclFloat16。 |
| incx | 输入 | x连续元素之间的步长。<br>预留参数，当前只能设置为-1。 |
| beta | 输入 | 用于执行乘操作的标量β的指针。类型定义请参见aclFloat16。 |
| y | 输入&输出 | 向量y的指针。类型定义请参见aclFloat16。 |
| incy | 输入 | y连续元素之间的步长。<br>预留参数，当前只能设置为-1。 |
| type | 输入 | 计算精度，默认高精度。类型定义请参见[aclComputeType](aclComputeType.md)。 |
| stream | 输入 | 执行算子所在的Stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
