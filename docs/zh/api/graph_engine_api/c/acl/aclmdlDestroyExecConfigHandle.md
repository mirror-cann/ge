# aclmdlDestroyExecConfigHandle

## 产品支持情况

<!-- npu="950" id601 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id601 -->
<!-- npu="A3" id602 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id602 -->
<!-- npu="910b" id603 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id603 -->
<!-- npu="310b" id604 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id604 -->
<!-- npu="310p" id605 -->
- Atlas 推理系列产品：支持
<!-- end id605 -->
<!-- npu="910" id606 -->
- Atlas 训练系列产品：支持
<!-- end id606 -->
<!-- npu="IPV350" id607 -->
- IPV350：支持
<!-- end id607 -->

## 功能说明

销毁通过[aclmdlCreateExecConfigHandle](aclmdlCreateExecConfigHandle.md)接口创建的aclmdlExecConfigHandle类型的数据。

## 函数原型

```c
aclError aclmdlDestroyExecConfigHandle(const aclmdlExecConfigHandle *handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| handle | 输入 | 待销毁的aclmdlExecConfigHandle类型的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
