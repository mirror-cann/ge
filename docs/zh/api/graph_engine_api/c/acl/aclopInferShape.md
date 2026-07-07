# aclopInferShape

## 产品支持情况

<!-- npu="950" id470 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id470 -->
<!-- npu="A3" id471 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id471 -->
<!-- npu="910b" id472 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id472 -->
<!-- npu="310b" id473 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id473 -->
<!-- npu="310p" id474 -->
- Atlas 推理系列产品：支持
<!-- end id474 -->
<!-- npu="910" id475 -->
- Atlas 训练系列产品：支持
<!-- end id475 -->
<!-- npu="IPV350" id476 -->
- IPV350：不支持
<!-- end id476 -->

## 功能说明

根据算子的输入Shape、输入值推导出算子的输出Shape。

推导算子的输出Shape包含三种场景：

- 根据Shape推导，可以得到算子的准确输出Shape，则返回准确输出Shape；
- 根据Shape推导，无法得到算子的准确输出Shape，但可以得到输出Shape的范围，则在输出参数outputDesc中将算子输出tensor描述中的动态维度的维度值记为-1。该场景下，用户可调用[aclGetTensorDescDimRange](aclGetTensorDescDimRange.md)接口获取tensor描述中指定维度的范围值。
- （该场景预留）根据Shape推导，无法得到算子的准确输出Shape以及Shape范围，则在输出参数outputDesc中将算子输出tensor描述中的动态维度的维度值记为-2。

## 函数原型

```c
aclError aclopInferShape(const char *opType,
int numInputs,
aclTensorDesc *inputDesc[],
aclDataBuffer *inputs[],
int numOutputs,
aclTensorDesc *outputDesc[],
aclopAttr *attr)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 算子类型名称的指针。 |
| numInputs | 输入 | 算子输入tensor的数量。 |
| inputDesc | 输入 | 算子输入tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>inputDesc数组中的元素个数必须与numInputs参数值保持一致。 |
| inputs | 输入 | 算子输入tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。 |
| numOutputs | 输入 | 算子输出tensor的数量。 |
| outputDesc | 输出 | 算子输出tensor描述的指针数组。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。<br>outputDesc数组中的元素个数必须与numOutputs参数值保持一致 |
| attr | 输入 | 算子属性。类型定义请参见[aclopAttr](aclopAttr.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
Ascend EP形态下，inputs参数处算子输入tensor数据的内存需申请Host上的内存。
<!-- end id1 -->

<!-- npu="310b" id2 -->
Ascend RC形态下，inputs参数处算子输入tensor数据的内存需申请Device上的内存。
<!-- end id2 -->

<!-- npu="310p" id3 -->
Control CPU开放形态下，inputs参数处算子输入tensor数据的内存需申请Device上的内存。
<!-- end id3 -->

如果算子有动态输入DYNAMIC\_INPUT或动态输出DYNAMIC\_OUTPUT，在调用aclopInferShape接口推导算子的输出Shape前，需先调用[aclSetTensorDescName](aclSetTensorDescName.md)接口设置所有输入和输出的tensor描述的名称，且名称必须按照如下要求：

- 对于必选输入、可选输入、必选输出，名称必须与算子IR原型中定义的输入/输出名称保持一致。
- 对于动态输入、动态输出，名称必须是：算子IR原型中定义的输入/输出名称+_编号_。编号根据动态输入/输出的个数确定，从0开始，0对应第一个动态输入/输出，1对应第二个动态输入/输出，以此类推。

例如某个算子有2个输入（第1个是必选输入x，第二个是动态输入y且输入个数为2）、1个必选输出z，则调用[aclSetTensorDescName](aclSetTensorDescName.md)接口设置名称的代码示例如下：

```c++
aclSetTensorDescName(inputTensorDesc[0], "x");
aclSetTensorDescName(inputTensorDesc[1], "y0");
aclSetTensorDescName(inputTensorDesc[2], "y1");
aclSetTensorDescName(outputTensorDesc[0], "z");
```
