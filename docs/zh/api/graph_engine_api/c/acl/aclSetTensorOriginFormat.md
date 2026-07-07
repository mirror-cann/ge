# aclSetTensorOriginFormat

## 产品支持情况

<!-- npu="950" id853 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id853 -->
<!-- npu="A3" id854 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id854 -->
<!-- npu="910b" id855 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id855 -->
<!-- npu="310b" id856 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id856 -->
<!-- npu="310p" id857 -->
- Atlas 推理系列产品：支持
<!-- end id857 -->
<!-- npu="910" id858 -->
- Atlas 训练系列产品：支持
<!-- end id858 -->
<!-- npu="IPV350" id859 -->
- IPV350：x
<!-- end id859 -->

## 功能说明

调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建tensor描述信息后，可通过aclSetTensorOriginFormat设置tensor的原始Format信息。

例如，某算子的原始Format为NHWC（原始Shape为4维）或者NDHWC（原始Shape为5维），为了提高数据访问效率，在进行模型转换或者在线构建网络时会自动转换Format，将原始数据Format转化为AI处理器内部计算数据时用的Format（NC1HWC0或者NDC1HWC0），同时进行推导Shape用于内部计算。

## 函数原型

```c
aclError aclSetTensorOriginFormat(aclTensorDesc *desc, aclFormat format)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| format | 输入 | 需要设置的format。类型定义请参见aclFormat。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
