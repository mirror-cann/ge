# aclGetTensorDescElementCount

## 产品支持情况

<!-- npu="950" id1000 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1000 -->
<!-- npu="A3" id1001 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1001 -->
<!-- npu="910b" id1002 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1002 -->
<!-- npu="310b" id1003 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1003 -->
<!-- npu="310p" id1004 -->
- Atlas 推理系列产品：支持
<!-- end id1004 -->
<!-- npu="910" id1005 -->
- Atlas 训练系列产品：支持
<!-- end id1005 -->
<!-- npu="IPV350" id1006 -->
- IPV350：不支持
<!-- end id1006 -->

## 功能说明

获取tensor描述中的元素个数。

## 函数原型

```c
size_t aclGetTensorDescElementCount(const aclTensorDesc *desc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |

## 返回值说明

返回指定tensor描述中的元素个数。
