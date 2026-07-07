# aclGetTensorDescSize

## 产品支持情况

<!-- npu="950" id127 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id127 -->
<!-- npu="A3" id128 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id128 -->
<!-- npu="910b" id129 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id129 -->
<!-- npu="310b" id130 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id130 -->
<!-- npu="310p" id131 -->
- Atlas 推理系列产品：支持
<!-- end id131 -->
<!-- npu="910" id132 -->
- Atlas 训练系列产品：支持
<!-- end id132 -->
<!-- npu="IPV350" id133 -->
- IPV350：不支持
<!-- end id133 -->

## 功能说明

获取tensor数据占用的空间大小。

## 函数原型

```c
size_t aclGetTensorDescSize(const aclTensorDesc *desc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |

## 返回值说明

返回指定tensor描述占用的空间大小。
