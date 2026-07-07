# aclopCompile

## 产品支持情况

<!-- npu="950" id1182 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id1182 -->
<!-- npu="A3" id1183 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1183 -->
<!-- npu="910b" id1184 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1184 -->
<!-- npu="310b" id1185 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1185 -->
<!-- npu="310p" id1186 -->
- Atlas 推理系列产品：支持
<!-- end id1186 -->
<!-- npu="910" id1187 -->
- Atlas 训练系列产品：支持
<!-- end id1187 -->
<!-- npu="IPV350" id1188 -->
- IPV350：不支持
<!-- end id1188 -->

## 功能说明

编译指定算子。在编译算子前，可以调用[aclSetCompileopt](aclSetCompileopt.md)接口设置编译选项。

每个算子的输入、输出组织不同，在调用接口时严格按照算子输入、输出参数来组织算子，用户在调用aclopCompile时，接口会根据optype、输入tensor的描述、输出tensor的描述、attr等信息查找对应的任务，再编译算子。

## 函数原型

```c
aclError aclopCompile(const char *opType,
int numInputs,
const aclTensorDesc *const inputDesc[],
int numOutputs,
const aclTensorDesc *const outputDesc[],
const aclopAttr *attr,
aclopEngineType engineType,
aclopCompileType compileFlag,
const char *opPath)
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
| engineType | 输入 | 算子执行引擎。类型定义请参见[aclopEngineType](aclopEngineType.md)。 |
| compileFlag | 输入 | 算子编译标识。类型定义请参见[aclopCompileType](aclopCompileType.md)。 |
| opPath | 输入 | 算子实现文件（*.py）的路径的指针，不包含文件名。预留参数，当前仅支持设置为nullptr。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

- 编译动态Shape的算子时，如果Shape具体值不明确，但Shape范围明确，则在调用aclCreateTensorDesc接口创建[aclTensorDesc](aclTensorDesc.md)类型时，需要将dims数组的表示动态维度的元素值设置为-1，再调用[aclSetTensorShapeRange](aclSetTensorShapeRange.md)接口设置tensor的各个维度的取值范围；如果Shape具体值以及Shape范围都不明确（该场景预留），则在调用aclCreateTensorDesc接口创建[aclTensorDesc](aclTensorDesc.md)类型时，需要将dims数组的值设置为-2，例如：int64\_t dims\[\] = \{-2\}。
- 编译有可选输入的算子时，如果可选输入不使用，则需调用aclCreateTensorDesc(ACL\_DT\_UNDEFINED, 0, nullptr, ACL\_FORMAT\_UNDEFINED)接口创建[aclTensorDesc](aclTensorDesc.md)类型的数据，将数据类型设置为ACL\_DT\_UNDEFINED，将Format设置为ACL\_FORMAT\_UNDEFINED，将Shape信息设置为nullptr。
- 编译有constant输入的算子时，需要先调用[aclSetTensorConst](aclSetTensorConst.md)接口设置constant输入。调用aclopCompile接口、[aclopExecuteV2](aclopExecuteV2.md)接口前，设置的constant输入数据必须保持一致。
