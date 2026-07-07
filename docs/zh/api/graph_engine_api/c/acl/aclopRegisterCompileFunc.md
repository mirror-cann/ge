# aclopRegisterCompileFunc

## 产品支持情况

<!-- npu="950" id685 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id685 -->
<!-- npu="A3" id686 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id686 -->
<!-- npu="910b" id687 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id687 -->
<!-- npu="310b" id688 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id688 -->
<!-- npu="310p" id689 -->
- Atlas 推理系列产品：支持
<!-- end id689 -->
<!-- npu="910" id690 -->
- Atlas 训练系列产品：支持
<!-- end id690 -->
<!-- npu="IPV350" id691 -->
- IPV350：不支持
<!-- end id691 -->

## 功能说明

动态Shape场景下，注册算子选择器，用于在算子执行时，能针对不同shape，选择相应的Tiling策略。

如果某算子已注册算子选择器，则不允许重新注册，如果需要变更算子选择器，必须先调用[aclopUnregisterCompileFunc](aclopUnregisterCompileFunc.md)接口取消注册，然后再调用aclopRegisterCompileFunc接口重新注册。

## 函数原型

```c
aclError aclopRegisterCompileFunc(const char *opType, aclopCompileFunc func)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 算子类型的指针。 |
| func | 输入 | 算子选择器回调函数。 |

<br>

函数定义如下：

```c
typedef aclError (*aclopCompileFunc)(int numInputs, const aclTensorDesc *const inputDesc[], int numOutputs, const aclTensorDesc *const outputDesc[], const aclopAttr *opAttr, aclopKernelDesc *aclopKernelDesc);
```

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
