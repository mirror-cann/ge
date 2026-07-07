# aclCreateTensorDesc

## 产品支持情况

<!-- npu="950" id993 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id993 -->
<!-- npu="A3" id994 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id994 -->
<!-- npu="910b" id995 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id995 -->
<!-- npu="310b" id996 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id996 -->
<!-- npu="310p" id997 -->
- Atlas 推理系列产品：支持
<!-- end id997 -->
<!-- npu="910" id998 -->
- Atlas 训练系列产品：支持
<!-- end id998 -->
<!-- npu="IPV350" id999 -->
- IPV350：不支持
<!-- end id999 -->

## 功能说明

创建aclTensorDesc类型的数据，该数据类型用于描述tensor的数据类型、shape、format等信息。

如需销毁aclTensorDesc类型的数据，请参见[aclDestroyTensorDesc](aclDestroyTensorDesc.md)。

## 函数原型

```c
aclTensorDesc *aclCreateTensorDesc(aclDataType dataType,
int numDims,
const int64_t *dims,
aclFormat format)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| dataType | 输入 | tensor描述的数据类型。类型定义请参见aclDataType。 |
| numDims | 输入 | tensor描述shape的维度个数。<br>numDims存在以下约束：<br><br>  - numDims必须大于或等于0；<br>  - numDims>0时，numDims的值必须与dims数组的长度保持一致；<br>  - numDims=0时，系统不使用dims数组值，dims参数值无效。 |
| dims | 输入 | tensor描述维度大小的指针。<br>dims是一个数组，数组中的每个元素表示tensor中每个维度的大小，如果数组中某个元素的值为0，则为空tensor。<br>如果用户需要使用空tensor，则在申请内存时，内存大小最小为1Byte，以保障后续业务正常运行。 |
| format | 输入 | tensor描述的format。类型定义请参见aclFormat。 |

## 返回值说明

返回[aclTensorDesc](aclTensorDesc.md)类型的指针。
