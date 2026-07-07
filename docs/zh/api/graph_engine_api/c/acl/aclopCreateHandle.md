# aclopCreateHandle

## 产品支持情况

<!-- npu="950" id986 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id986 -->
<!-- npu="A3" id987 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id987 -->
<!-- npu="910b" id988 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id988 -->
<!-- npu="310b" id989 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id989 -->
<!-- npu="310p" id990 -->
- Atlas 推理系列产品：支持
<!-- end id990 -->
<!-- npu="910" id991 -->
- Atlas 训练系列产品：支持
<!-- end id991 -->
<!-- npu="IPV350" id992 -->
- IPV350：不支持
<!-- end id992 -->
- MC62CM12A AI处理器：不支持

## 功能说明

创建一个执行算子的handle。

如需销毁handle，请参见[aclopDestroyHandle](aclopDestroyHandle.md)。

## 函数原型

```c
aclError aclopCreateHandle(const char *opType,
int numInputs,
const aclTensorDesc *const inputDesc[],
int numOutputs,
const aclTensorDesc *const outputDesc[],
const aclopAttr *opAttr,
aclopHandle **handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 算子类型名称的指针。 |
| numInputs | 输入 | 算子输入tensor的数量。 |
| inputDesc | 输入 | 算子输入tensor描述的指针数组。 |
| numOutputs | 输入 | 算子输出tensor的数量。 |
| outputDesc | 输入 | 算子输出tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。 |
| opAttr | 输入 | 算子属性的指针。类型定义请参见[aclopAttr](aclopAttr.md)。 |
| handle | 输出 | “aclopHandle数据指针”的指针。类型定义请参见[aclopHandle](aclopHandle.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
## 约束说明

对于算子有constant输入的场景，如果未调用[aclSetTensorConst](aclSetTensorConst.md)接口设置constant输入，则需调用[aclSetTensorPlaceMent](aclSetTensorPlaceMent.md)设置TensorDesc的placement属性，将memType设置为Host内存。
<!-- end id1 -->
