# aclSetTensorConst

## 产品支持情况

<!-- npu="950" id120 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id120 -->
<!-- npu="A3" id121 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id121 -->
<!-- npu="910b" id122 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id122 -->
<!-- npu="310b" id123 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id123 -->
<!-- npu="310p" id124 -->
- Atlas 推理系列产品：支持
<!-- end id124 -->
<!-- npu="910" id125 -->
- Atlas 训练系列产品：支持
<!-- end id125 -->
<!-- npu="IPV350" id126 -->
- IPV350：不支持
<!-- end id126 -->

## 功能说明

为算子设置constant输入，指定存放输入tensor数据的内存地址和地址长度。

## 函数原型

```c
aclError aclSetTensorConst(aclTensorDesc *desc, void *dataBuffer, size_t length)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| dataBuffer | 输入 | 输入tensor数据内存地址指针。 |
| length | 输入 | 内存地址的长度，单位是Byte。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
Ascend EP形态下，dataBuffer处需申请Host上的内存。
<!-- end id1 -->

<!-- npu="310b" id2 -->
Ascend RC形态下，dataBuffer处需申请Device上的内存。
<!-- end id2 -->

<!-- npu="310p" id3 -->
Control CPU开放形态下，dataBuffer处需申请Device上的内存。
<!-- end id3 -->

<!-- npu="IPV350" id4 -->
dataBuffer处需申请Device上的内存。
<!-- end id4 -->
