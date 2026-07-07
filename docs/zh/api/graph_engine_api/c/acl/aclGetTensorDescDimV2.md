# aclGetTensorDescDimV2

## 产品支持情况

<!-- npu="950" id818 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id818 -->
<!-- npu="A3" id819 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id819 -->
<!-- npu="910b" id820 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id820 -->
<!-- npu="310b" id821 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id821 -->
<!-- npu="310p" id822 -->
- Atlas 推理系列产品：支持
<!-- end id822 -->
<!-- npu="910" id823 -->
- Atlas 训练系列产品：支持
<!-- end id823 -->
<!-- npu="IPV350" id824 -->
- IPV350：不支持
<!-- end id824 -->

## 功能说明

获取tensor描述中指定维度的大小。

## 函数原型

```c
aclError aclGetTensorDescDimV2(const aclTensorDesc *desc, size_t index, int64_t *dimSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| index | 输入 | 指定获取第几个维度的大小，index值从0开始。<br>用户调用[aclGetTensorDescNumDims](aclGetTensorDescNumDims.md)接口获取shape维度个数，这个Index的取值范围：[0, (shape维度个数-1)]。 |
| dimSize | 输出 | tensor描述中index指定维度大小的指针。<br>当返回的dimSize参数值为-1时，表示维度的大小是动态的。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

当[aclGetTensorDescNumDims](aclGetTensorDescNumDims.md)接口的返回值为ACL\_UNKNOWN\_RANK时，表示动态Shape场景下维度个数未知，则不能调用aclGetTensorDescDimV2接口获取指定维度的大小。
