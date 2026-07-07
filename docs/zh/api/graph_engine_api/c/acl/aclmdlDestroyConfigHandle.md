# aclmdlDestroyConfigHandle

## 产品支持情况

<!-- npu="950" id1028 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1028 -->
<!-- npu="A3" id1029 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1029 -->
<!-- npu="910b" id1030 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1030 -->
<!-- npu="310b" id1031 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1031 -->
<!-- npu="310p" id1032 -->
- Atlas 推理系列产品：支持
<!-- end id1032 -->
<!-- npu="910" id1033 -->
- Atlas 训练系列产品：支持
<!-- end id1033 -->
<!-- npu="IPV350" id1034 -->
- IPV350：支持
<!-- end id1034 -->

## 功能说明

销毁通过[aclmdlCreateConfigHandle](aclmdlCreateConfigHandle.md)接口创建的aclmdlConfigHandle类型的数据。

## 函数原型

```c
aclError aclmdlDestroyConfigHandle(aclmdlConfigHandle *handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| handle | 输入 | 待销毁的aclmdlConfigHandle类型的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
