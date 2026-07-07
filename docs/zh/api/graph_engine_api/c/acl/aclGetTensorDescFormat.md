# aclGetTensorDescFormat

## 产品支持情况

<!-- npu="950" id43 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id43 -->
<!-- npu="A3" id44 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id44 -->
<!-- npu="910b" id45 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id45 -->
<!-- npu="310b" id46 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id46 -->
<!-- npu="310p" id47 -->
- Atlas 推理系列产品：支持
<!-- end id47 -->
<!-- npu="910" id48 -->
- Atlas 训练系列产品：支持
<!-- end id48 -->
<!-- npu="IPV350" id49 -->
- IPV350：不支持
<!-- end id49 -->

## 功能说明

获取tensor描述中的format。

## 函数原型

```c
aclFormat aclGetTensorDescFormat(const aclTensorDesc *desc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |

## 返回值说明

返回指定tensor描述的format，类型为aclFormat。
