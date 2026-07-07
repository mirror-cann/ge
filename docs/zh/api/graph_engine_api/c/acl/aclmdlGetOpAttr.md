# aclmdlGetOpAttr

## 产品支持情况

<!-- npu="950" id1007 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1007 -->
<!-- npu="A3" id1008 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1008 -->
<!-- npu="910b" id1009 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1009 -->
<!-- npu="310b" id1010 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1010 -->
<!-- npu="310p" id1011 -->
- Atlas 推理系列产品：支持
<!-- end id1011 -->
<!-- npu="910" id1012 -->
- Atlas 训练系列产品：支持
<!-- end id1012 -->
<!-- npu="IPV350" id1013 -->
- IPV350：不支持
<!-- end id1013 -->

## 功能说明

获取整网中某个模型中某个算子的属性的值。

## 函数原型

```c
const char *aclmdlGetOpAttr(aclmdlDesc *modelDesc, const char *opName, const char *attr)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据，再调用[aclmdlGetDesc](aclmdlGetDesc.md)接口根据模型ID获取到对应的aclmdlDesc类型的数据。 |
| opName | 输入 | 算子名称的指针。 |
| attr | 输入 | 算子属性的指针。<br>当前仅支持_datadump_original_op_names属性，用于记录某个算子是由哪些算子融合得到的。通过本接口获取到的_datadump_original_op_names属性值格式为[opName1_len]opName1…..[opNameN_len]opNameN，opNameN_len表示算子名称字符串的长度。<br>_datadump_original_op_names属性值示例如下，表示某个融合算子由scale2c_branch2c、bn2c_branch2c、res2c_branch2c、res2c、res2c_relu这五个算子融合而成的，算子名称字符串的长度分别为16、13、14、5、10：<br>[16]scale2c_branch2c[13]bn2c_branch2c[14]res2c_branch2c[5]res2c[10]res2c_relu |

## 返回值说明

返回属性值的字符串指针，该指针的生命周期与modelDesc相同，若modelDesc资源被销毁，则该指针指向的内容也会自动被销毁。若opName或者attr属性不存在、或者attr属性值为空，均返回指向空字符串的指针。

若调用该接口失败，则返回nullptr。
