# aclmdlGetNumInputs

## 产品支持情况

<!-- npu="950" id211 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id211 -->
<!-- npu="A3" id212 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id212 -->
<!-- npu="910b" id213 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id213 -->
<!-- npu="310b" id214 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id214 -->
<!-- npu="310p" id215 -->
- Atlas 推理系列产品：支持
<!-- end id215 -->
<!-- npu="910" id216 -->
- Atlas 训练系列产品：支持
<!-- end id216 -->
<!-- npu="IPV350" id217 -->
- IPV350：支持
<!-- end id217 -->

## 功能说明

根据模型描述信息获取模型的输入个数。

## 函数原型

```c
size_t aclmdlGetNumInputs(aclmdlDesc *modelDesc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |

## 返回值说明

模型的输入个数。
