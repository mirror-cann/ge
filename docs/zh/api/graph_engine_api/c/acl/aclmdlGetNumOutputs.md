# aclmdlGetNumOutputs

## 产品支持情况

<!-- npu="950" id1014 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1014 -->
<!-- npu="A3" id1015 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1015 -->
<!-- npu="910b" id1016 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1016 -->
<!-- npu="310b" id1017 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1017 -->
<!-- npu="310p" id1018 -->
- Atlas 推理系列产品：支持
<!-- end id1018 -->
<!-- npu="910" id1019 -->
- Atlas 训练系列产品：支持
<!-- end id1019 -->
<!-- npu="IPV350" id1020 -->
- IPV350：支持
<!-- end id1020 -->

## 功能说明

根据模型描述信息获取模型的输出个数。

## 函数原型

```c
size_t aclmdlGetNumOutputs(aclmdlDesc *modelDesc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |

## 返回值说明

模型的输出个数。
