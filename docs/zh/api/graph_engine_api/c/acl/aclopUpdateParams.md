# aclopUpdateParams

## 产品支持情况

<!-- npu="950" id260 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id260 -->
<!-- npu="A3" id261 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id261 -->
<!-- npu="910b" id262 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id262 -->
<!-- npu="310b" id263 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id263 -->
<!-- npu="310p" id264 -->
- Atlas 推理系列产品：支持
<!-- end id264 -->
<!-- npu="910" id265 -->
- Atlas 训练系列产品：支持
<!-- end id265 -->
<!-- npu="IPV350" id266 -->
- IPV350：不支持
<!-- end id266 -->

## 功能说明

在算子动态Shape场景下，根据指定算子，触发调用算子选择器。

## 函数原型

```c
aclError aclopUpdateParams(const char *opType,
int numInputs,
const aclTensorDesc *const inputDesc[],
int numOutputs,
const aclTensorDesc *const outputDesc[],
const aclopAttr *attr)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 算子类型名称的指针。 |
| numInputs | 输入 | 算子输入tensor的数量。 |
| inputDesc | 输入 | 算子输入tensor的描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>inputDesc数组中的元素个数必须与numInputs参数值保持一致。 |
| numOutputs | 输入 | 算子输出tensor的数量。 |
| outputDesc | 输入 | 算子输出tensor的描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>outputDesc数组中的元素个数必须与numOutputs参数值保持一致。 |
| attr | 输入 | 算子属性的指针。类型定义请参见[aclopAttr](aclopAttr.md)。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
