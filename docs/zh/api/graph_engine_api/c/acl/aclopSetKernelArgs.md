# aclopSetKernelArgs

## 产品支持情况

<!-- npu="950" id1105 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id1105 -->
<!-- npu="A3" id1106 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1106 -->
<!-- npu="910b" id1107 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1107 -->
<!-- npu="310b" id1108 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1108 -->
<!-- npu="310p" id1109 -->
- Atlas 推理系列产品：支持
<!-- end id1109 -->
<!-- npu="910" id1110 -->
- Atlas 训练系列产品：支持
<!-- end id1110 -->
<!-- npu="IPV350" id1111 -->
- IPV350：不支持
<!-- end id1111 -->

## 功能说明

动态Shape场景下，设置算子Tiling参数、执行并发数。

## 函数原型

```c
aclError aclopSetKernelArgs(aclopKernelDesc *kernelDesc,
const char *kernelId,
uint32_t blockDim,
const void *args,
uint32_t argSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| kernelDesc | 输入 | Kernel描述缓存，aclopKernelDesc类型的指针。<br>typedef struct aclopKernelDesc aclopKernelDesc; |
| kernelId | 输入 | 算子执行时要指定的Kernel ID的指针，与调用[aclopCreateKernel](aclopCreateKernel.md)时传递的kernelId一致。 |
| blockDim | 输入 | Kernel执行的并发AI Core核数。<br>建议此处设置的blockDim和TIK算子实现时使用的AI Core核数保持一致。 |
| args | 输入 | Tiling参数的指针。 |
| argSize | 输入 | Tiling参数内存大小，单位为Byte。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
