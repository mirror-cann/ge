# aclmdlCreateAndGetOpDesc

## 产品支持情况

<!-- npu="950" id50 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id50 -->
<!-- npu="A3" id51 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id51 -->
<!-- npu="910b" id52 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id52 -->
<!-- npu="310b" id53 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id53 -->
<!-- npu="310p" id54 -->
- Atlas 推理系列产品：支持
<!-- end id54 -->
<!-- npu="910" id55 -->
- Atlas 训练系列产品：支持
<!-- end id55 -->
<!-- npu="IPV350" id56 -->
- IPV350：不支持
<!-- end id56 -->

## 功能说明

获取指定算子的描述信息，包括算子名、输入tensor描述、输出tensor描述，如果查询不到指定算子，则返回报错。该接口不支持动态Shape场景。

**使用场景举例**：执行整网模型推理时，如果产生AI Core报错，可以调用本接口获取报错算子的描述信息，再做进一步错误排查。

**推荐的接口调用顺序如下：**

1. 定义并实现异常回调函数fn，回调函数原型请参见aclrtSetExceptionInfoCallback。

    实现回调函数的关键步骤如下：

    1. 在异常回调函数fn内调用aclrtGetDeviceIdFromExceptionInfo、aclrtGetStreamIdFromExceptionInfo、aclrtGetTaskIdFromExceptionInfo接口分别获取Device ID、Stream ID、Task ID。
    2. 在异常回调函数fn内调用[aclmdlCreateAndGetOpDesc](aclmdlCreateAndGetOpDesc.md)接口获取算子的描述信息。
    3. 在异常回调函数fn内调用[aclGetTensorDescByIndex](aclGetTensorDescByIndex.md)接口获取指定算子输入/输出的tensor描述。
    4. 在异常回调函数fn内调用如下接口获取tensor描述中的数据，进行进一步分析。

        例如，调用[aclGetTensorDescAddress](aclGetTensorDescAddress.md)接口获取tensor数据的内存地址（用户可从该内存地址中获取tensor数据）、调用[aclGetTensorDescType](aclGetTensorDescType.md)接口获取tensor描述中的数据类型、调用[aclGetTensorDescFormat](aclGetTensorDescFormat.md)接口获取tensor描述中的Format、调用[aclGetTensorDescNumDims](aclGetTensorDescNumDims.md)接口获取tensor描述中的Shape维度个数、调用[aclGetTensorDescDimV2](aclGetTensorDescDimV2.md)接口获取Shape中指定维度的大小。

2. 调用aclrtSetExceptionInfoCallback接口设置异常回调函数。
3. 执行模型推理。

    如果存在AI Core报错，则触发回调函数fn，获取算子的信息，进行进一步分析。

## 函数原型

```c
aclError aclmdlCreateAndGetOpDesc(uint32_t deviceId, uint32_t streamId, uint32_t taskId, char *opName, size_t opNameLen, aclTensorDesc **inputDesc, size_t *numInputs, aclTensorDesc **outputDesc, size_t *numOutputs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| deviceId | 输入 | Device ID。<br>调用aclrtGetDeviceIdFromExceptionInfo接口获取异常信息中的Device ID，作为本接口的输入。 |
| streamId | 输入 | Stream ID。<br>调用aclrtGetStreamIdFromExceptionInfo接口获取异常信息中的Stream ID，作为本接口的输入。 |
| taskId | 输入 | Task ID。<br>调用aclrtGetTaskIdFromExceptionInfo接口获取异常信息中的Task ID，作为本接口的输入。 |
| opName | 输出 | 算子名称字符串的指针。 |
| opNameLen | 输入 | 算子名称字符串长度。<br>若用户指定的长度比实际算子名称的长度短，则返回报错。 |
| inputDesc | 输出 | 算子所有输入的tensor描述的指针，指向一块连续内存的首地址。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。 |
| numInputs | 输出 | 输入个数的指针。 |
| outputDesc | 输出 | 算子所有输出的tensor描述的指针，指向一块连续内存的首地址。类型定义请参见[aclTensorDesc](aclTensorDesc.md)。 |
| numOutputs | 输出 | 输出个数的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
