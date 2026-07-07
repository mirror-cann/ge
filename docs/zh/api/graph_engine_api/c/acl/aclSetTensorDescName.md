# aclSetTensorDescName

## 产品支持情况

<!-- npu="950" id1112 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1112 -->
<!-- npu="A3" id1113 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1113 -->
<!-- npu="910b" id1114 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1114 -->
<!-- npu="310b" id1115 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1115 -->
<!-- npu="310p" id1116 -->
- Atlas 推理系列产品：支持
<!-- end id1116 -->
<!-- npu="910" id1117 -->
- Atlas 训练系列产品：支持
<!-- end id1117 -->
<!-- npu="IPV350" id1118 -->
- IPV350：不支持
<!-- end id1118 -->

## 功能说明

设置TensorDesc的名称。

## 函数原型

```c
void aclSetTensorDescName(aclTensorDesc *desc, const char *name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| name | 输入 | TensorDesc名称的指针。 |

## 返回值说明

无
