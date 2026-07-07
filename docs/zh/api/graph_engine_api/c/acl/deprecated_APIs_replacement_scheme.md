# 废弃接口替换方案

本章中的单算子模型执行接口、CBLAS接口后续版本会废弃，若继续使用，可能导致未定义的行为，请使用aclnn算子接口替代。

## 单算子模型执行接口替换方案

单算子模型执行接口主要包括编译算子（如aclopCompile接口）、加载算子模型文件（如aclopLoad接口）以及执行算子（如aclopExecuteV2接口）等核心步骤。对于不同的算子，均调用该接口集，但入参需根据算子的输入、输出和属性等信息进行构造。**因此，在替换单算子模型执行接口时，直接选用对应的aclnn算子接口即可。**例如，原先使用单算子模型执行接口调用Add算子，现可使用aclnnAdd算子接口进行替换。

## CBLAS接口替换方案

CBLAS接口主要分为两类功能：执行矩阵-向量乘法和执行矩阵-矩阵乘法。其核心步骤包括：使用ATC工具编译算子、加载算子模型文件（如aclopLoad接口）以及执行算子（例如aclblasGemvEx）。矩阵-向量乘法和矩阵-矩阵乘法对应不同的执行接口，且输入输出数据类型也存在差异。**因此CBLAS接口与aclnn算子接口之间不存在直接的一一对应关系，替换时需要通过Matmul、Muls和Add等aclnn基础算子接口的组合来实现，样例代码请参见[Link](https://gitcode.com/cann/ge/blob/master/examples/acl/4_sample_acl_gemm/README.md)。**

具体替换方案如下：

|  |  |  |
| --- | --- | --- |
| 新/老接口 | CBLAS接口 | aclnn组合方案 |
| 接口名称 | aclblasGemvEx<br>aclblasCreateHandleForGemvEx | aclnnMatmul(n=1) + aclnnMuls + aclnnAdd |
| 数据类型 | FLOAT16/FLOAT | FLOAT16/FLOAT |
| 公式 | y = αAx + βy | 四步：Matmul → Muls → Muls → Add |

|  |  |  |
| --- | --- | --- |
| 新/老接口 | CBLAS接口 | aclnn 组合方案 |
| 接口名称 | aclblasHgemv<br>aclblasCreateHandleForHgemv | aclnnMatmul(n=1) + aclnnMuls + aclnnAdd |
| 数据类型 | FLOAT16（固定） | FLOAT16（固定） |
| 公式 | y = αAx + βy | 四步：Matmul → Muls → Muls → Add |

|  |  |  |
| --- | --- | --- |
| 新/老接口 | CBLAS接口 | aclnn 组合方案 |
| 接口名称 | aclblasS8gemv<br>aclblasCreateHandleForS8gemv | aclnnQuantMatmulV5(n=1) + aclnnMuls + aclnnAdd |
| 数据类型 | INT8 × INT8输入 → INT32输出 | INT8输入 → INT32输出 |
| 公式 | y = αAx + βy | 四步：QuantMatmulV5 → Muls(INT32) → Muls(INT32) → Add |

|  |  |  |
| --- | --- | --- |
| 新/老接口 | CBLAS接口 | aclnn 组合方案 |
| 接口名称 | aclblasGemmEx<br>aclblasCreateHandleForGemmEx | aclnnMatmul + aclnnMuls + aclnnAdd |
| 数据类型 | FLOAT16/FLOAT | FLOAT16/FLOAT |
| 公式 | C = alpha × op(A) × op(B) + beta × C | 四步：Matmul → Muls(alpha) → Muls(beta) → Add |

|  |  |  |
| --- | --- | --- |
| 新/老接口 | aclblas | aclnn 组合方案 |
| 接口名称 | aclblasHgemm<br>aclblasCreateHandleForHgemm | aclnnMatmul + aclnnMuls + aclnnAdd |
| 数据类型 | FLOAT（固定） | FLOAT（固定） |
| 公式 | C = alpha × op(A) × op(B) + beta × C | 四步：Matmul → Muls(alpha) → Muls(beta) → Add |

|  |  |  |
| --- | --- | --- |
| 新/老接口 | aclblas | aclnn 组合方案 |
| 接口名称 | aclblasS8gemm<br>aclblasCreateHandleForS8gemm | aclnnQuantMatmulV5 + aclnnMuls + aclnnAdd |
| 数据类型 | INT8 × INT8输入 → INT32输出 | INT8输入 → INT32输出 |
| 公式 | C = alpha × A × B + beta × C | 四步：QuantMatmulV5 → Muls(INT32) → Muls(INT32) → Add |
