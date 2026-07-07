# aclmdlSetAttribute

## 产品支持情况

<!-- npu="950" id881 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id881 -->
<!-- npu="A3" id882 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id882 -->
<!-- npu="910b" id883 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id883 -->
<!-- npu="310b" id884 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id884 -->
<!-- npu="310p" id885 -->
- Atlas 推理系列产品：支持
<!-- end id885 -->
<!-- npu="910" id886 -->
- Atlas 训练系列产品：支持
<!-- end id886 -->
<!-- npu="IPV350" id887 -->
- IPV350：支持
<!-- end id887 -->

## 功能说明

设置模型属性。

加载模型成功后，可调用本接口修改模型的属性值。

## 函数原型

```c
aclError aclmdlSetAttribute(uint32_t modelId, aclmdlAttr attr, aclmdlAttrValue_t *attrValue)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 指定模型的ID。<br>调用模型加载接口（例如[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口、[aclmdlLoadFromMem](aclmdlLoadFromMem.md)等）成功后，会返回模型ID，该ID作为本接口的输入。 |
| attr | 输入 | 指定需设置的属性。类型定义请参见[aclmdlAttr](aclmdlAttr.md)。 |
| attrValue | 输入 | 指向属性值的指针，attr对应的属性取值。类型定义请参见[aclmdlAttrValue_t](aclmdlAttrValue_t.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
