# aclmdlGetDesc

## 产品支持情况

<!-- npu="950" id162 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id162 -->
<!-- npu="A3" id163 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id163 -->
<!-- npu="910b" id164 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id164 -->
<!-- npu="310b" id165 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id165 -->
<!-- npu="310p" id166 -->
- Atlas 推理系列产品：支持
<!-- end id166 -->
<!-- npu="910" id167 -->
- Atlas 训练系列产品：支持
<!-- end id167 -->
<!-- npu="IPV350" id168 -->
- IPV350：支持
<!-- end id168 -->

## 功能说明

根据模型ID获取该模型的模型描述信息。

## 函数原型

```c
aclError aclmdlGetDesc(aclmdlDesc *modelDesc, uint32_t modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输出 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| modelId | 输入 | 模型ID。<br>调用[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口/[aclmdlLoadFromMem](aclmdlLoadFromMem.md)接口/[aclmdlLoadFromFileWithMem](aclmdlLoadFromFileWithMem.md)接口/[aclmdlLoadFromMemWithMem](aclmdlLoadFromMemWithMem.md)接口加载模型成功后，会返回模型ID。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
