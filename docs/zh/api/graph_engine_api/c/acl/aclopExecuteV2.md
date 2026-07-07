# aclopExecuteV2

## 产品支持情况

<!-- npu="950" id8 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id8 -->
<!-- npu="A3" id9 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id9 -->
<!-- npu="910b" id10 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id10 -->
<!-- npu="310b" id11 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id11 -->
<!-- npu="310p" id12 -->
- Atlas 推理系列产品：支持
<!-- end id12 -->
<!-- npu="910" id13 -->
- Atlas 训练系列产品：支持
<!-- end id13 -->
<!-- npu="IPV350" id14 -->
- IPV350：不支持
<!-- end id14 -->

## 功能说明

执行指定的算子。异步接口。

每个算子的输入、输出、属性不同，接口会根据optype、输入tensor的描述、输出tensor的描述、attr等信息查找对应的任务，并下发执行，因此在调用本接口时需严格按照算子输入、输出、属性来组织算子。

## 函数原型

```c
aclError aclopExecuteV2(const char *opType,
int numInputs,
aclTensorDesc *inputDesc[],
aclDataBuffer *inputs[],
int numOutputs,
aclTensorDesc *outputDesc[],
aclDataBuffer *outputs[],
aclopAttr *attr,
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
| outputDesc | 输入&输出 | 算子输出tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>outputDesc数组中的元素个数必须与numOutputs参数值保持一致，且outputs数组与outputDesc数组中的元素必须一一对应。 |
| outputs | 输出 | 算子输出tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>outputs数组中的元素个数必须与numOutputs参数值保持一致，且outputs数组与outputDesc数组中的元素必须一一对应。 |
| attr | 输入 | 算子属性的指针。类型定义请参见[aclopAttr](aclopAttr.md)。<br>需提前调用[aclopCreateAttr](aclopCreateAttr.md)接口创建aclopAttr类型。 |
| stream | 输入 | 该算子需要加载的stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

- 多线程场景下，不支持调用本接口时指定同一个Stream或使用默认Stream，否则可能任务执行异常。

- 对于支持动态Shape的算子，无法明确算子输出Shape时，可调用[aclopInferShape](aclopInferShape.md)接口获取算子的输出Shape：
    - 若可获取到准确的输出Shape，则使用准确的输出Shape来构造outputDesc参数，作为aclopExecuteV2的输入。该场景下，aclopExecuteV2接口是**异步接口**，对于异步接口，调用接口成功仅表示任务下发成功，不表示任务执行成功。调用该接口后，需调用同步等待接口（例如，aclrtSynchronizeStream）确保任务已执行完成。
    - 若无法获取准确的输出Shape，仅能获取输出Shape范围，则使用Shape最大值来构造outputDesc参数，作为aclopExecuteV2的输入。该场景下，在调用aclopExecuteV2接口执行算子后，系统内部会计算出准确的输出Shape，通过aclopExecuteV2接口的outputDesc参数输出，此时aclopExecuteV2接口是**同步接口**。
    - （该场景预留）若无法获取准确的输出Shape以及Shape范围，则需由用户预估一个最大的Shape来构造outputDesc参数，作为aclopExecuteV2的输入。该场景下，在调用aclopExecuteV2接口执行算子后，系统内部会计算出准确的输出Shape，通过aclopExecuteV2接口的outputDesc参数输出，此时aclopExecuteV2接口是**同步接口**。

- 执行有可选输入的算子时，如果可选输入不使用：
    - 则需调用aclCreateTensorDesc(ACL\_DT\_UNDEFINED, 0, nullptr, ACL\_FORMAT\_UNDEFINED)接口创建aclTensorDesc类型的数据，将数据类型设置为ACL\_DT\_UNDEFINED，将Format设置为ACL\_FORMAT\_UNDEFINED，将Shape信息设置为nullptr。
    - 则需调用aclCreateDataBuffer(nullptr, 0)接口创建aclDataBuffer类型的数据，同时aclDataBuffer中的数据不需要释放，因为是空指针。

- 执行有constant输入的算子时，需要先调用[aclSetTensorConst](aclSetTensorConst.md)接口设置constant输入。

    调用[aclopCompile](aclopCompile.md)接口、aclopExecuteV2接口时，设置的constant输入数据必须保持一致。

    <!-- npu="950,A3,910b,910,310p,310b" id1 -->
    对于算子有constant输入的场景，如果未调用[aclSetTensorConst](aclSetTensorConst.md)接口设置constant输入，则需调用[aclSetTensorPlaceMent](aclSetTensorPlaceMent.md)设置TensorDesc的placement属性，将memType设置为Host内存。
    <!-- end id1 -->

<!-- npu="950,A3,910b,910,310p,310b" id2 -->
- 在执行单算子时，一般要求用户传入的输入/输出tensor数据是存放在Device内存的，比如两个tensor相加的场景。但是，也存在部分算子，除了将featureMap、weight等Device内存中的tensor数据作为输入外，还需tensor shape信息、learningRate、tensor的dims信息等通常在Host内存中的tensor数据也作为输入，此时，用户不需要额外将这些Host内存中的tensor数据拷贝到Device上作为输入，只需要调用[aclSetTensorPlaceMent](aclSetTensorPlaceMent.md)接口设置对应TensorDesc的placement属性为Host内存，在执行单算子时，设置了placement属性为Host内存的TensorDesc，其对应的tensor数据必须存放在Host内存中，接口内部会自动将Host上的tensor数据拷贝到Device上。
<!-- end id2 -->
