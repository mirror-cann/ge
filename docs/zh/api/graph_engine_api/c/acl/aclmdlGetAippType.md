# aclmdlGetAippType

## 产品支持情况

<!-- npu="950" id1161 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1161 -->
<!-- npu="A3" id1162 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1162 -->
<!-- npu="910b" id1163 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1163 -->
<!-- npu="310b" id1164 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1164 -->
<!-- npu="310p" id1165 -->
- Atlas 推理系列产品：支持
<!-- end id1165 -->
<!-- npu="910" id1166 -->
- Atlas 训练系列产品：支持
<!-- end id1166 -->
<!-- npu="IPV350" id1167 -->
- IPV350：不支持
<!-- end id1167 -->

## 功能说明

获取指定模型的指定输入所支持的AIPP类型（动态AIPP或静态AIPP）及动态AIPP输入对应的index值。

## 函数原型

```c
aclError aclmdlGetAippType(uint32_t modelId, size_t index, aclmdlInputAippType *type, size_t *dynamicAttachedDataIndex)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 指定模型的ID。<br>调用[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口/[aclmdlLoadFromMem](aclmdlLoadFromMem.md)接口/[aclmdlLoadFromFileWithMem](aclmdlLoadFromFileWithMem.md)接口/[aclmdlLoadFromMemWithMem](aclmdlLoadFromMemWithMem.md)接口加载模型成功后，会返回模型ID。 |
| index | 输入 | 模型中输入的index。 |
| type | 输出 | 指定模型输入的AIPP类型的指针。类型定义请参见[aclmdlInputAippType](aclmdlInputAippType.md)。 |
| dynamicAttachedDataIndex | 输出 | 当type不为ACL_DATA_WITH_DYNAMIC_AIPP时，该值返回0xFFFFFFFF，表示无效。<br>当type为ACL_DATA_WITH_DYNAMIC_AIPP时，该值返回动态AIPP输入（用于配置动态AIPP参数）的index。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
