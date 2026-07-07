# aclGetTensorDescByIndex

## 产品支持情况

<!-- npu="950" id1203 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1203 -->
<!-- npu="A3" id1204 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1204 -->
<!-- npu="910b" id1205 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1205 -->
<!-- npu="310b" id1206 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1206 -->
<!-- npu="310p" id1207 -->
- Atlas 推理系列产品：支持
<!-- end id1207 -->
<!-- npu="910" id1208 -->
- Atlas 训练系列产品：支持
<!-- end id1208 -->
<!-- npu="IPV350" id1209 -->
- IPV350：不支持
<!-- end id1209 -->

## 功能说明

获取算子的指定Index的tensor描述信息。

## 函数原型

```c
aclTensorDesc *aclGetTensorDescByIndex(aclTensorDesc *desc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>调用[aclmdlCreateAndGetOpDesc](aclmdlCreateAndGetOpDesc.md)接口获取算子所有输入/输出的tensor描述，作为本接口的输入。 |
| index | 输入 | 指定获取第几个输入/输出的tensor描述，index值从0开始。<br>用户调用[aclmdlCreateAndGetOpDesc](aclmdlCreateAndGetOpDesc.md)接口获取的算子输入/输出的数量后，这个Index的取值范围：[0, (算子输入/输出的数量-1)]。 |

## 返回值说明

返回指定输入/输出的[aclTensorDesc](aclTensorDesc.md)类型数据的指针。
