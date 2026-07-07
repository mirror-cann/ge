# aclblasCreateHandleForGemvEx

## 产品支持情况

<!-- npu="950" id979 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id979 -->
<!-- npu="A3" id980 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id980 -->
<!-- npu="910b" id981 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id981 -->
<!-- npu="310b" id982 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id982 -->
<!-- npu="310p" id983 -->
- Atlas 推理系列产品：支持
<!-- end id983 -->
<!-- npu="910" id984 -->
- Atlas 训练系列产品：支持
<!-- end id984 -->
<!-- npu="IPV350" id985 -->
- IPV350：不支持
<!-- end id985 -->

## 功能说明

创建矩阵-向量乘的handle，输入数据、输出数据的数据类型通过入参设置。创建handle成功后，需调用[aclopExecWithHandle](aclopExecWithHandle.md)接口执行算子。

A、x、y的数据类型仅支持以下组合：

| A的数据类型 | x的数据类型 | y的数据类型 |
| --- | --- | --- |
| aclFloat16 | aclFloat16 | aclFloat16 |
| aclFloat16 | aclFloat16 | float(float32) |
| int8_t | int8_t | float(float32) |
| int8_t | int8_t | int32_t |

## 函数原型

```c
aclError aclblasCreateHandleForGemvEx(aclTransType transA,
int m,
int n,
aclDataType dataTypeA,
aclDataType dataTypeX,
aclDataType dataTypeY,
aclComputeType type,
aclopHandle **handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| transA | 输入 | A矩阵是否转置的标记。类型定义请参见[aclTransType](aclTransType.md)。 |
| m | 输入 | 矩阵A的行数，存储矩阵乘数据时，行优先。 |
| n | 输入 | 矩阵A的列数。 |
| dataTypeA | 输入 | 矩阵A的数据类型。类型定义请参见aclDataType。 |
| dataTypeX | 输入 | 向量x的数据类型。类型定义请参见aclDataType。 |
| dataTypeY | 输入 | 向量y的数据类型。类型定义请参见aclDataType。 |
| type | 输入 | 计算精度，默认高精度。类型定义请参见[aclComputeType](aclComputeType.md)。 |
| handle | 输出 | “算子执行handle的指针”的指针。类型定义请参见[aclopHandle](aclopHandle.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
