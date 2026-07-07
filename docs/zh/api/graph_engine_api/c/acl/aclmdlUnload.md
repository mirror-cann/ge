# aclmdlUnload

## 产品支持情况

<!-- npu="950" id1042 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1042 -->
<!-- npu="A3" id1043 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1043 -->
<!-- npu="910b" id1044 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1044 -->
<!-- npu="310b" id1045 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1045 -->
<!-- npu="310p" id1046 -->
- Atlas 推理系列产品：支持
<!-- end id1046 -->
<!-- npu="910" id1047 -->
- Atlas 训练系列产品：支持
<!-- end id1047 -->
<!-- npu="IPV350" id1048 -->
- IPV350：支持
<!-- end id1048 -->

## 功能说明

系统完成模型推理后，可调用本接口卸载模型，释放资源，但需确保其它接口没有正在使用该模型。

模型加载、模型执行、模型卸载的操作必须在同一个Context下（关于Context的创建请参见aclrtCreateContext）。

## 函数原型

```c
aclError aclmdlUnload(uint32_t modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 需卸载的模型的ID。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
