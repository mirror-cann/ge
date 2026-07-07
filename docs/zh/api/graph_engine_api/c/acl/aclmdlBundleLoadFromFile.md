# aclmdlBundleLoadFromFile

## 产品支持情况

<!-- npu="950" id358 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id358 -->
<!-- npu="A3" id359 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id359 -->
<!-- npu="910b" id360 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id360 -->
<!-- npu="310b" id361 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id361 -->
<!-- npu="310p" id362 -->
- Atlas 推理系列产品：支持
<!-- end id362 -->
<!-- npu="910" id363 -->
- Atlas 训练系列产品：支持
<!-- end id363 -->
<!-- npu="IPV350" id364 -->
- IPV350：不支持
<!-- end id364 -->

## 功能说明

模型执行阶段若涉及动态更新变量的场景，可调用本接口从文件加载om模型，由系统内部管理内存。

**本接口需与以下其它接口配合使用**，以便实现动态更新变量的目的，关键接口的调用流程如下：

1. 基于构图接口编译并保存模型，模型中包含多个图，例如推理图、变量初始化图、变量更新图等。

    此处是调用aclgrphBundleBuildModel接口编译模型、调用aclgrphBundleSaveModel接口保存模型，接口详细描述参见[《图开发》](https://hiascend.com/document/redirect/CannCommunityGraphguide)中的“接口参考 \> C++语言接口 \> aclgrph接口 \> aclgrphBundleBuildModel”、“接口参考 \> C++语言接口 \> aclgrph接口 \> aclgrphBundleSaveModel”。

2. 调用[aclmdlBundleLoadFromFile](aclmdlBundleLoadFromFile.md)或[aclmdlBundleLoadFromMem](aclmdlBundleLoadFromMem.md)接口加载模型。
3. 调用[aclmdlBundleGetModelId](aclmdlBundleGetModelId.md)接口获取多个图的ID。
4. 根据多个图的ID，分别调用模型执行接口（例如[aclmdlExecute](aclmdlExecute.md)）执行各个图。

    若涉及变量更新，则在执行变量更新图之前，先调用[aclmdlSetDatasetTensorDesc](aclmdlSetDatasetTensorDesc.md)接口设置图的tensor描述信息，再执行变量更新图，然后再执行一次推理图。

5. 推理结束后，调用[aclmdlBundleUnload](aclmdlBundleUnload.md)接口卸载模型。

## 函数原型

```c
aclError aclmdlBundleLoadFromFile(const char *modelPath, uint32_t *bundleId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelPath | 输入 | 模型文件路径的指针，路径中包含文件名。运行程序（APP）的用户需要对该存储路径有访问权限。<br>此处的模型文件是基于构图接口构建出来的，调用aclgrphBundleBuildModel接口编译模型、调用aclgrphBundleSaveModel接口保存模型。 |
| bundleId | 输出 | 系统成功加载模型后，返回bundleId作为后续操作时识别模型的标志。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
