# aclmdlBundleQueryInfoFromFile

## 产品支持情况

<!-- npu="950" id657 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id657 -->
<!-- npu="A3" id658 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id658 -->
<!-- npu="910b" id659 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id659 -->
<!-- npu="310b" id660 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id660 -->
<!-- npu="310p" id661 -->
- Atlas 推理系列产品：支持
<!-- end id661 -->
<!-- npu="910" id662 -->
- Atlas 训练系列产品：支持
<!-- end id662 -->
<!-- npu="IPV350" id663 -->
- IPV350：不支持
<!-- end id663 -->

## 功能说明

模型执行阶段，如果涉及动态更新变量的场景，可以调用本接口从模型文件获取其描述信息。

## 函数原型

```c
aclError aclmdlBundleQueryInfoFromFile(const char* fileName, aclmdlBundleQueryInfo *queryInfo)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| fileName | 输入 | 模型文件路径的指针，路径中包含文件名。运行程序（APP）的用户需要对该存储路径有访问权限。<br>此处的模型文件是基于构图接口构建出来的，调用aclgrphBundleBuildModel接口编译模型、调用aclgrphBundleSaveModel接口保存模型，接口详细描述参见[《图开发》](https://hiascend.com/document/redirect/CannCommunityGraphguide)中的“接口参考 > C++语言接口 > aclgrph接口 > aclgrphBundleBuildModel”。 |
| queryInfo | 输出 | 需提前调用[aclmdlBundleCreateQueryInfo](aclmdlBundleCreateQueryInfo.md)接口创建aclmdlBundleQueryInfo类型的数据。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
