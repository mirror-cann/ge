# aclopExecute（废弃）

**须知：此接口后续版本会废弃，请使用[aclopExecuteV2](aclopExecuteV2.md)接口。**

## 产品支持情况

<!-- npu="950" id155 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id155 -->
<!-- npu="A3" id156 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id156 -->
<!-- npu="910b" id157 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id157 -->
<!-- npu="310b" id158 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id158 -->
<!-- npu="310p" id159 -->
- Atlas 推理系列产品：支持
<!-- end id159 -->
<!-- npu="910" id160 -->
- Atlas 训练系列产品：支持
<!-- end id160 -->
<!-- npu="IPV350" id161 -->
- IPV350：不支持
<!-- end id161 -->

## 功能说明

执行指定的算子。异步接口。

每个算子的输入、输出、属性不同，接口会根据optype、输入tensor的描述、输出tensor的描述、attr等信息查找对应的任务，并下发执行，因此在调用本接口时需严格按照算子输入、输出、属性来组织算子。

## 函数原型

```c
aclError aclopExecute(const char *opType,
int numInputs,
const aclTensorDesc *const inputDesc[],
const aclDataBuffer *const inputs[],
int numOutputs,
const aclTensorDesc *const outputDesc[],
aclDataBuffer *const outputs[],
const aclopAttr *attr,
aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 算子类型名称的指针。 |
| numInputs | 输入 | 算子输入tensor的数量。 |
| inputDesc | 输入 | 算子输入tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>inputDesc数组中的元素个数必须与numInputs参数值保持一致，且inputs数组与inputDesc数组中的元素必须一一对应。 |
| inputs | 输入 | 算子输入tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>inputs数组中的元素个数必须与numInputs参数值保持一致，且inputs数组与inputDesc数组中的元素必须一一对应。 |
| numOutputs | 输入 | 算子输出tensor的数量。<br>outputDesc数组中的元素个数必须与numOutputs参数值保持一致。 |
| outputDesc | 输入 | 算子输出tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>outputDesc数组中的元素个数必须与numOutputs参数值保持一致，且outputs数组与outputDesc数组中的元素必须一一对应。 |
| outputs | 输出 | 算子输出tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>outputs数组中的元素个数必须与numOutputs参数值保持一致，且outputs数组与outputDesc数组中的元素必须一一对应。 |
| attr | 输入 | 算子属性的指针。类型定义请参见[aclopAttr](aclopAttr.md)。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| stream | 输入 | 该算子需要加载的stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

多线程场景下，不支持调用本接口时指定同一个Stream或使用默认Stream，否则可能任务执行异常。
