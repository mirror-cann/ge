# aclmdlGetInputDynamicGearCount

## 产品支持情况

<!-- npu="950" id1063 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1063 -->
<!-- npu="A3" id1064 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1064 -->
<!-- npu="910b" id1065 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1065 -->
<!-- npu="310b" id1066 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1066 -->
<!-- npu="310p" id1067 -->
- Atlas 推理系列产品：支持
<!-- end id1067 -->
<!-- npu="910" id1068 -->
- Atlas 训练系列产品：支持
<!-- end id1068 -->
<!-- npu="IPV350" id1069 -->
- IPV350：支持
<!-- end id1069 -->

## 功能说明

根据模型描述信息获取模型的输入所支持的动态维度档位数。

## 函数原型

```c
aclError aclmdlGetInputDynamicGearCount(const aclmdlDesc *modelDesc, size_t index, size_t *gearCount)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 预留参数，当前未使用，固定设置为-1。 |
| gearCount | 输出 | 动态维度档位数的指针。<br>例如，模型的输入tensor是4维，在模型转换时通过--dynamic_dims参数设置的分档为“1,3,224,224;2,3,224,224;1,3,256,256”,那么通过该接口获取的动态维度档位数为3。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

如果模型编译时没有设置动态维度的分档，那么通过该接口获取的动态维度档位数为0。
