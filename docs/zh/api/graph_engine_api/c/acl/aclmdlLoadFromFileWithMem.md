# aclmdlLoadFromFileWithMem

## 产品支持情况

<!-- npu="950" id302 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id302 -->
<!-- npu="A3" id303 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id303 -->
<!-- npu="910b" id304 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id304 -->
<!-- npu="310b" id305 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id305 -->
<!-- npu="310p" id306 -->
- Atlas 推理系列产品：支持
<!-- end id306 -->
<!-- npu="910" id307 -->
- Atlas 训练系列产品：支持
<!-- end id307 -->
<!-- npu="IPV350" id308 -->
- IPV350：不支持
<!-- end id308 -->

## 功能说明

从文件加载离线模型数据，由用户自行管理模型运行的内存。

本接口中通过modelPath参数传入的文件是\*.om模型文件。关于如何获取om模型文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 \> 基础功能参数 \> 总体选项 \> --mode”。

## 函数原型

```c
aclError aclmdlLoadFromFileWithMem(const char *modelPath, uint32_t *modelId, void *workPtr, size_t workSize, void *weightPtr, size_t weightSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelPath | 输入 | 模型文件路径的指针，路径中包含文件名。运行程序（APP）的用户需要对该存储路径有访问权限。 |
| modelId | 输出 | 模型ID的指针。<br>系统成功加载模型后，返回模型ID作为后续操作时识别模型的标志。 |
| workPtr | 输入 | Device上模型所需工作内存（存放模型执行过程中的临时数据）的地址指针，由用户自行管理，模型执行过程中不能释放该内存。<br>如果在workPtr参数处传入空指针，表示由系统管理内存。<br>由用户自行管理工作内存时，如果多个模型串行执行，支持共用同一个工作内存，但用户需确保模型的串行执行顺序、且工作内存的大小需按多个模型中最大工作内存的大小来申请，例如通过以下方式保证串行：模型执行时，加锁，保证执行任务串行。 |
| workSize | 输入 | 模型所需工作内存的大小，单位Byte。workPtr为空指针时无效。 |
| weightPtr | 输入 | Device上模型权值内存（存放权值数据）的地址指针，由用户自行管理，模型执行过程中不能释放该内存。<br>如果在weightPtr参数处传入空指针，表示由系统管理内存。<br>由用户自行管理权值内存时，在多线程场景下，对于同一个模型，如果在每个线程中都加载了一次，支持共用weightPtr，因为weightPtr内存在推理过程中是只读的。此处需注意，在共用weightPtr期间，不能释放weightPtr。 |
| weightSize | 输入 | 模型所需权值内存的大小，单位Byte。weightPtr为空指针时无效。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 参考资源

当前还提供了[aclmdlSetConfigOpt](aclmdlSetConfigOpt.md)接口、[aclmdlLoadWithConfig](aclmdlLoadWithConfig.md)接口来实现模型加载，通过配置对象中的属性来区分，在加载模型时是从文件加载，还是从内存加载，以及内存是由系统内部管理，还是由用户管理。
