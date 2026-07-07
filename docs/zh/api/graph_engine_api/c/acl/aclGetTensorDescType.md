# aclGetTensorDescType

## 产品支持情况

<!-- npu="950" id99 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id99 -->
<!-- npu="A3" id100 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id100 -->
<!-- npu="910b" id101 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id101 -->
<!-- npu="310b" id102 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id102 -->
<!-- npu="310p" id103 -->
- Atlas 推理系列产品：支持
<!-- end id103 -->
<!-- npu="910" id104 -->
- Atlas 训练系列产品：支持
<!-- end id104 -->
<!-- npu="IPV350" id105 -->
- IPV350：不支持
<!-- end id105 -->

## 功能说明

获取tensor描述中的数据类型。

## 函数原型

```c
aclDataType aclGetTensorDescType(const aclTensorDesc *desc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |

## 返回值说明

返回指定tensor描述的数据类型，类型为aclDataType。
