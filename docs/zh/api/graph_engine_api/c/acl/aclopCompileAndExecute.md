# aclopCompileAndExecute

## 产品支持情况

<!-- npu="950" id253 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id253 -->
<!-- npu="A3" id254 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id254 -->
<!-- npu="910b" id255 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id255 -->
<!-- npu="310b" id256 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id256 -->
<!-- npu="310p" id257 -->
- Atlas 推理系列产品：支持
<!-- end id257 -->
<!-- npu="910" id258 -->
- Atlas 训练系列产品：支持
<!-- end id258 -->
<!-- npu="IPV350" id259 -->
- IPV350：不支持
<!-- end id259 -->

## 功能说明

编译并执行指定的算子，当前只支持固定Shape的算子。异步接口。

每个算子的输入、输出、属性不同，接口会根据optype、输入tensor的描述、输出tensor的描述、attr等信息查找对应的任务，并下发执行，因此在调用本接口时需严格按照算子输入、输出、属性来组织算子。

在编译执行算子前，可以调用[aclSetCompileopt](aclSetCompileopt.md)接口设置编译选项。

## 函数原型

```c
aclError aclopCompileAndExecute(const char *opType,
int numInputs,
const aclTensorDesc *const inputDesc[],
const aclDataBuffer *const inputs[],
int numOutputs,
const aclTensorDesc *const outputDesc[],
aclDataBuffer *const outputs[],
const aclopAttr *attr,
aclopEngineType engineType,
aclopCompileType compileFlag,
const char *opPath,
aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 算子类型名称的指针。 |
| numInputs | 输入 | 算子输入tensor的数量。 |
| inputDesc | 输入 | 算子输入tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>inputDesc数组中的元素个数必须与numInputs参数值保持一致，且inputs数组与inputDesc数组中的元素必须一一对应。 |
| inputs | 输入 | 算子输入tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>inputs数组中的元素个数必须与numInputs参数值保持一致，且inputs数组与inputDesc数组中的元素必须一一对应。 |
| numOutputs | 输入 | 算子输出tensor的数量。 |
| outputDesc | 输入 | 算子输出tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>outputDesc数组中的元素个数必须与numOutputs参数值保持一致，且outputs数组与outputDesc数组中的元素必须一一对应。 |
| outputs | 输入&输出 | 算子输出tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>outputs数组中的元素个数必须与numOutputs参数值保持一致，且outputs数组与outputDesc数组中的元素必须一一对应。 |
| attr | 输入 | 算子属性的指针。类型定义请参见[aclopAttr](aclopAttr.md)。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| engineType | 输入 | 算子执行引擎。类型定义请参见[aclopEngineType](aclopEngineType.md)。 |
| compileFlag | 输入 | 算子编译标识。类型定义请参见[aclopCompileType](aclopCompileType.md)。 |
| opPath | 输入 | 算子实现文件（*.py）路径的指针，不包含文件名。预留参数，当前仅支持设置为nullptr。 |
| stream | 输入 | 该算子需要加载的stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

- 多线程场景下，不支持调用本接口时指定同一个Stream或使用默认Stream，否则可能任务执行异常。
- 每个算子的输入、输出、属性不同，需要应用在调用时严格按照算子输入、输出、属性来组织算子，用户在调用aclopCompileAndExecute时，根据optype、输入tensor的描述、输出tensor的描述、attr等信息查找对应的任务，再编译、执行算子。
- 编译、执行有可选输入的算子时，如果可选输入不使用，则：
    - 需调用aclCreateTensorDesc(ACL\_DT\_UNDEFINED, 0, nullptr, ACL\_FORMAT\_UNDEFINED)接口创建aclTensorDesc类型的数据，将数据类型设置为ACL\_DT\_UNDEFINED，将Format设置为ACL\_FORMAT\_UNDEFINED，将Shape信息设置为nullptr。
    - 需调用aclCreateDataBuffer(nullptr, 0)接口创建aclDataBuffer类型的数据，同时aclDataBuffer中的数据不需要释放，因为是空指针。

- 编译、执行有constant输入的算子时，需要先调用[aclSetTensorConst](aclSetTensorConst.md)接口设置constant输入。

    <!-- npu="950,A3,910b,910,310p,310b" id1 -->
    对于算子有constant输入的场景，如果未调用[aclSetTensorConst](aclSetTensorConst.md)接口设置constant输入，则需调用[aclSetTensorPlaceMent](aclSetTensorPlaceMent.md)设置TensorDesc的placement属性，将memType设置为Host内存。
    <!-- end id1 -->

<!-- npu="950,A3,910b,910,310p,310b" id2 -->
- 在执行单算子时，一般要求用户传入的输入/输出tensor数据是存放在Device内存的，比如两个tensor相加的场景。但是，也存在部分算子，除了将featureMap、weight等Device内存中的tensor数据作为输入外，还需tensor shape信息、learningRate、tensor的dims信息等通常在Host内存中的tensor数据也作为输入，此时，用户不需要额外将这些Host内存中的tensor数据拷贝到Device上作为输入，只需要调用[aclSetTensorPlaceMent](aclSetTensorPlaceMent.md)接口设置对应TensorDesc的placement属性为Host内存，在执行单算子时，设置了placement属性为Host内存的TensorDesc，其对应的tensor数据必须存放在Host内存中，接口内部会自动将Host上的tensor数据拷贝到Device上。
<!-- end id2 -->
