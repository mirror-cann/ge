# aclSetTensorOriginShape

## 产品支持情况

<!-- npu="950" id477 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id477 -->
<!-- npu="A3" id478 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id478 -->
<!-- npu="910b" id479 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id479 -->
<!-- npu="310b" id480 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id480 -->
<!-- npu="310p" id481 -->
- Atlas 推理系列产品：支持
<!-- end id481 -->
<!-- npu="910" id482 -->
- Atlas 训练系列产品：支持
<!-- end id482 -->
<!-- npu="IPV350" id483 -->
- IPV350：x
<!-- end id483 -->

## 功能说明

调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建tensor描述信息后，可通过aclSetTensorOriginShape接口设置tensor的原始dims信息。

例如，某算子的原始Format为NHWC（原始Shape为4维）或者NDHWC（原始Shape为5维），为了提高数据访问效率，在进行模型转换或者在线构建网络时会自动转换Format，将原始数据Format转化为AI处理器内部计算数据时用的Format（NC1HWC0或者NDC1HWC0），同时进行推导Shape用于内部计算。

## 函数原型

```c
aclError aclSetTensorOriginShape(aclTensorDesc *desc, int numDims, const int64_t *dims)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| numDims | 输入 | 需要设置的dims维度数。 |
| dims | 输入 | dims的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
