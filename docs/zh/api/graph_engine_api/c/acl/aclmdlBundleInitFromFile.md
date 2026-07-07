# aclmdlBundleInitFromFile

## 产品支持情况

<!-- npu="950" id518 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id518 -->
<!-- npu="A3" id519 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id519 -->
<!-- npu="910b" id520 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id520 -->
<!-- npu="310b" id521 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id521 -->
<!-- npu="310p" id522 -->
- Atlas 推理系列产品：支持
<!-- end id522 -->
<!-- npu="910" id523 -->
- Atlas 训练系列产品：支持
<!-- end id523 -->
<!-- npu="IPV350" id524 -->
- IPV350：不支持
<!-- end id524 -->

## 功能说明

在模型执行阶段，如果涉及动态更新变量的场景，可以调用本接口从模型文件中初始化模型。

## 函数原型

```c
aclError aclmdlBundleInitFromFile(const char* modelPath, void *varWeightPtr, size_t varWeightSize, uint32_t *bundleId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelPath | 输入 | 模型文件路径的指针，路径中包含文件名。运行程序（APP）的用户需要对该存储路径有访问权限。<br>此处的模型文件是基于构图接口构建出来的，调用aclgrphBundleBuildModel接口编译模型、调用aclgrphBundleSaveModel接口保存模型，接口详细描述参见[《图开发》](https://hiascend.com/document/redirect/CannCommunityGraphguide)中的“接口参考 > C++语言接口 > aclgrph接口 > aclgrphBundleBuildModel”。 |
| varWeightPtr | 输入 | 模型所需的可刷新权重内存的地址指针，由用户自行管理，模型执行过程中不能释放该内存。<br>如果在此处传入空指针，表示由系统管理内存。 |
| varWeightSize | 输入 | 模型所需的可刷新权重内存的大小，单位Byte。varWeightPtr为空指针时无效。 |
| bundleId | 输出 | 系统成功初始化模型后，返回bundleId作为后续操作时识别模型的标志。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
