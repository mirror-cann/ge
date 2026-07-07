# aclmdlBundleQueryInfoFromMem

## 产品支持情况

<!-- npu="950" id1098 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1098 -->
<!-- npu="A3" id1099 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1099 -->
<!-- npu="910b" id1100 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1100 -->
<!-- npu="310b" id1101 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1101 -->
<!-- npu="310p" id1102 -->
- Atlas 推理系列产品：支持
<!-- end id1102 -->
<!-- npu="910" id1103 -->
- Atlas 训练系列产品：支持
<!-- end id1103 -->
<!-- npu="IPV350" id1104 -->
- IPV350：不支持
<!-- end id1104 -->

## 功能说明

模型执行阶段，如果涉及动态更新变量的场景，可以调用本接口从内存中获取模型描述信息。

## 函数原型

```c
aclError aclmdlBundleQueryInfoFromMem(const void *model, size_t modelSize, aclmdlBundleQueryInfo *queryInfo)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model | 输入 | 存放模型数据的内存地址指针。<br>此处的模型文件是基于构图接口构建出来的，调用aclgrphBundleBuildModel接口编译模型、调用aclgrphBundleSaveModel接口保存模型，再由用户自行将保存出来的om模型文件读入内存，构图接口详细描述参见《图开发》中的“接口参考 > C++语言接口 > aclgrph接口 > aclgrphBundleBuildModel”。 |
| modelSize | 输入 | 内存中的模型数据长度，单位Byte。 |
| queryInfo | 输出 | 需提前调用[aclmdlBundleCreateQueryInfo](aclmdlBundleCreateQueryInfo.md)接口创建aclmdlBundleQueryInfo类型的数据。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
Ascend EP形态下，model参数处需申请Host上的内存。
<!-- end id1 -->

<!-- npu="310b" id2 -->
Ascend RC形态下，model参数处需申请Device上的内存。
<!-- end id2 -->

<!-- npu="310p" id3 -->
Control CPU开放形态下，model参数处需申请Device上的内存。
<!-- end id3 -->
