# aclSetTensorDynamicInput

## 产品支持情况

<!-- npu="950" id22 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id22 -->
<!-- npu="A3" id23 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id23 -->
<!-- npu="910b" id24 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id24 -->
<!-- npu="310b" id25 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id25 -->
<!-- npu="310p" id26 -->
- Atlas 推理系列产品：支持
<!-- end id26 -->
<!-- npu="910" id27 -->
- Atlas 训练系列产品：支持
<!-- end id27 -->
<!-- npu="IPV350" id28 -->
- IPV350：x
<!-- end id28 -->

## 功能说明

调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建tensor描述信息后，可通过aclSetTensorDynamicInput接口设置多个动态输入的分组标识，例如某个算子包括动态输入为x、必选输入y，其中动态输入有多个，调用aclSetTensorDynamicInput将**dynamicInputName**设置为x，接口内部根据**dynamicInputName**索引到对应的多个动态输入，达到分组的目的**。**

## 函数原型

```c
aclError aclSetTensorDynamicInput(aclTensorDesc *desc, const char *dynamicInputName)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| dynamicInputName | 输入 | desc中动态输入分组标识的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
