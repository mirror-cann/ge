# aclmdlGetInputNameByIndex

## 产品支持情况

<!-- npu="950" id316 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id316 -->
<!-- npu="A3" id317 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id317 -->
<!-- npu="910b" id318 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id318 -->
<!-- npu="310b" id319 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id319 -->
<!-- npu="310p" id320 -->
- Atlas 推理系列产品：支持
<!-- end id320 -->
<!-- npu="910" id321 -->
- Atlas 训练系列产品：支持
<!-- end id321 -->
<!-- npu="IPV350" id322 -->
- IPV350：支持
<!-- end id322 -->

## 功能说明

根据模型描述信息获取模型中指定输入的输入名称。

## 函数原型

```c
const char *aclmdlGetInputNameByIndex(const aclmdlDesc *modelDesc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输入的名称，index值从0开始。 |

## 返回值说明

返回指定输入的输入名称。
