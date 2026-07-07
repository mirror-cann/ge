# aclopExecWithHandle

## 产品支持情况

<!-- npu="950" id190 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id190 -->
<!-- npu="A3" id191 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id191 -->
<!-- npu="910b" id192 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id192 -->
<!-- npu="310b" id193 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id193 -->
<!-- npu="310p" id194 -->
- Atlas 推理系列产品：支持
<!-- end id194 -->
<!-- npu="910" id195 -->
- Atlas 训练系列产品：支持
<!-- end id195 -->
<!-- npu="IPV350" id196 -->
- IPV350：不支持
<!-- end id196 -->

## 功能说明

以Handle方式调用一个算子，不支持动态Shape算子，动态Shape算子请使用[aclopExecuteV2](aclopExecuteV2.md)。异步接口。

## 函数原型

```c
aclError aclopExecWithHandle(aclopHandle *handle,
int numInputs,
const aclDataBuffer *const inputs[],
int numOutputs,
aclDataBuffer *const outputs[],
aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| handle | 输入 | 算子执行handle的指针。类型定义请参见[aclopHandle](aclopHandle.md)。<br>需提前调用[aclopCreateHandle](aclopCreateHandle.md)接口创建aclopHandle类型的数据。 |
| numInputs | 输入 | 算子输入tensor的数量。 |
| inputs | 输入 | 算子输入tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>inputs数组中的元素个数必须与numInputs参数值保持一致。 |
| numOutputs | 输入 | 算子输出tensor的数量。 |
| outputs | 输出 | 算子输出tensor的指针数组。类型定义请参见aclDataBuffer。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。<br>outputs数组中的元素个数必须与numOutputs参数值保持一致 |
| stream | 输入 | 该算子需要加载的stream。类型定义请参见aclrtStream。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

- 多线程场景下，不支持调用本接口时指定同一个Stream或使用默认Stream，否则可能任务执行异常。
- 执行有可选输入的算子时，如果可选输入不使用，则需调用aclCreateDataBuffer(nullptr, 0)接口创建aclDataBuffer类型的数据，同时aclDataBuffer中的数据不需要释放，因为是空指针。
