# aclGetTensorDescAddress

## 产品支持情况

<!-- npu="950" id1126 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1126 -->
<!-- npu="A3" id1127 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1127 -->
<!-- npu="910b" id1128 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1128 -->
<!-- npu="310b" id1129 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1129 -->
<!-- npu="310p" id1130 -->
- Atlas 推理系列产品：支持
<!-- end id1130 -->
<!-- npu="910" id1131 -->
- Atlas 训练系列产品：支持
<!-- end id1131 -->
<!-- npu="IPV350" id1132 -->
- IPV350：不支持
<!-- end id1132 -->

## 功能说明

获取指定算子输入/输出的tensor数据的内存地址。

## 函数原型

```c
void *aclGetTensorDescAddress(const aclTensorDesc *desc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>调用[aclGetTensorDescByIndex](aclGetTensorDescByIndex.md)接口获取算子的指定输入/输出的tensor描述，作为本接口的输入。 |

## 返回值说明

返回指定算子输入/输出的tensor数据的内存地址。
