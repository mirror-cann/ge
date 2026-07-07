# aclTransTensorDescFormat

**须知：本接口为预留接口，暂不支持。**

## 产品支持情况

<!-- npu="950" id92 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id92 -->
<!-- npu="A3" id93 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id93 -->
<!-- npu="910b" id94 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id94 -->
<!-- npu="310b" id95 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id95 -->
<!-- npu="310p" id96 -->
- Atlas 推理系列产品：支持
<!-- end id96 -->
<!-- npu="910" id97 -->
- Atlas 训练系列产品：支持
<!-- end id97 -->
<!-- npu="IPV350" id98 -->
- IPV350：不支持
<!-- end id98 -->

## 功能说明

按指定dstFormat转换源aclTensorDesc中的format，生成新的目标aclTensorDesc，源aclTensorDesc中的format保持不变。同步接口。

## 函数原型

```c
aclError aclTransTensorDescFormat(const aclTensorDesc *srcDesc, aclFormat dstFormat, aclTensorDesc **dstDesc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| srcDesc | 输入 | 源aclTensorDesc数据的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| dstFormat | 输入 | 需要设置的目标format。类型定义请参见aclFormat。 |
| dstDesc | 输出 | “目标aclTensorDesc数据指针”的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
