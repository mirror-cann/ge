# aclGenGraphAndDumpForOp

## 产品支持情况

<!-- npu="950" id281 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id281 -->
<!-- npu="A3" id282 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id282 -->
<!-- npu="910b" id283 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id283 -->
<!-- npu="310b" id284 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id284 -->
<!-- npu="310p" id285 -->
- Atlas 推理系列产品：支持
<!-- end id285 -->
<!-- npu="910" id286 -->
- Atlas 训练系列产品：支持
<!-- end id286 -->
<!-- npu="IPV350" id287 -->
- IPV350：不支持
<!-- end id287 -->

## 功能说明

构建单个算子的图并Dump，生成.txt文件。此处的图是指表达算子IR（Intermediate Representation）的结构，图编译过程中由于融合、拆分等原因会导致图不断变换、进而生成新的图，这些变换的目的是为了将用户表达的图适配到AI处理器上，获得更高性能。

**使用场景**：算子调优场景下，调用本接口构建单个算子的图并Dump，生成.txt文件，作为算子调优的输入数据之一。

## 函数原型

```c
aclError aclGenGraphAndDumpForOp(const char *opType,
int numInputs,
const aclTensorDesc *const inputDesc[],
const aclDataBuffer *const inputs[],
int numOutputs,
const aclTensorDesc *const outputDesc[],
aclDataBuffer *const outputs[],
const aclopAttr *attr,
aclopEngineType engineType,
const char *graphDumpPath,
const aclGraphDumpOption *graphDumpOpt)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 指定算子类型名称的指针。 |
| numInputs | 输入 | 算子输入tensor的数量。 |
| inputDesc | 输入 | 算子输入tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>inputDesc数组中的元素个数必须与numInputs参数值保持一致，且inputs数组与inputDesc数组中的元素必须一一对应。 |
| inputs | 输入 | 算子输入tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>inputs数组中的元素个数必须与numInputs参数值保持一致，且inputs数组与inputDesc数组中的元素必须一一对应。 |
| numOutputs | 输入 | 算子输出tensor的数量。 |
| outputDesc | 输入 | 算子输出tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>outputDesc数组中的元素个数必须与numOutputs参数值保持一致，且outputs数组与outputDesc数组中的元素必须一一对应。 |
| outputs | 输入 | 算子输出tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>outputs数组中的元素个数必须与numOutputs参数值保持一致，且outputs数组与outputDesc数组中的元素必须一一对应。 |
| attr | 输入 | 算子属性的指针。类型定义请参见[aclopAttr](aclopAttr.md)。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| engineType | 输入 | 算子执行引擎。类型定义请参见[aclopEngineType](aclopEngineType.md)。 |
| graphDumpPath | 输入 | Dump单算子图所生成的文件（文件名后缀为.txt）的保存路径的指针。类型定义请参见[aclGraphDumpOption](aclGraphDumpOption.md)。<br>路径需要由用户提前创建，路径中如果不指定文件名，则由接口内部指定文件名。<br>如果多次Dump时，指定同一个路径，且文件名相同，则最新一次的文件会覆盖之前的文件。 |
| graphDumpOpt | 输入 | 单算子图的dump选项的指针。<br>当前该参数预留，仅支持传入nullptr。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
