# aclSetTensorStorageFormat（废弃）

**须知：此接口后续版本会废弃，请使用[aclSetTensorFormat](aclSetTensorFormat.md)接口。**

## 产品支持情况

<!-- npu="950" id553 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id553 -->
<!-- npu="A3" id554 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id554 -->
<!-- npu="910b" id555 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id555 -->
<!-- npu="310b" id556 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id556 -->
<!-- npu="310p" id557 -->
- Atlas 推理系列产品：支持
<!-- end id557 -->
<!-- npu="910" id558 -->
- Atlas 训练系列产品：x
<!-- end id558 -->
<!-- npu="IPV350" id559 -->
- IPV350：x
<!-- end id559 -->

## 功能说明

调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建tensor描述信息后，可通过aclSetTensorStorageFormat设置tensor的实际Format信息。

## 函数原型

```c
aclError aclSetTensorStorageFormat(aclTensorDesc *desc, aclFormat format)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| format | 输入 | 需要设置的format。类型定义请参见aclFormat。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
