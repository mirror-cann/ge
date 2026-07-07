# aclmdlLoadFromMemWithMem

## 产品支持情况

<!-- npu="950" id699 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id699 -->
<!-- npu="A3" id700 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id700 -->
<!-- npu="910b" id701 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id701 -->
<!-- npu="310b" id702 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id702 -->
<!-- npu="310p" id703 -->
- Atlas 推理系列产品：支持
<!-- end id703 -->
<!-- npu="910" id704 -->
- Atlas 训练系列产品：支持
<!-- end id704 -->
<!-- npu="IPV350" id705 -->
- IPV350：不支持
<!-- end id705 -->

## 功能说明

从内存加载om模型文件数据，由用户自行管理模型运行的内存。

关于如何获取om模型文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 \> 基础功能参数 \> 总体选项 \> --mode”。

## 函数原型

```c
aclError aclmdlLoadFromMemWithMem(const void *model, size_t modelSize, uint32_t *modelId, void *workPtr, size_t workSize, void *weightPtr, size_t weightSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model | 输入 | 存放模型数据的内存地址指针。 |
| modelSize | 输入 | 模型数据长度，单位Byte。 |
| modelId | 输出 | 模型ID的指针。<br>系统成功加载模型后，返回模型ID作为后续操作时识别模型的标志。 |
| workPtr | 输入 | Device上模型所需工作内存（存放模型执行过程中的临时数据）的地址指针，由用户自行管理，模型执行过程中不能释放该内存。<br>如果在workPtr参数处传入空指针，表示由系统管理内存。<br>由用户自行管理工作内存时，如果多个模型串行执行，支持共用同一个工作内存，但用户需确保模型的串行执行顺序、且工作内存的大小需按多个模型中最大工作内存的大小来申请，例如通过以下方式保证串行：模型执行时，加锁，保证执行任务串行。 |
| workSize | 输入 | 模型所需工作内存的大小，单位Byte。workPtr为空指针时无效。 |
| weightPtr | 输入 | Device上模型权值内存（存放权值数据）的地址指针，由用户自行管理，模型执行过程中不能释放该内存。<br>如果在weightPtr参数处传入空指针，表示由系统管理内存。<br>由用户自行管理权值内存时，在多线程场景下，对于同一个模型，如果在每个线程中都加载了一次，支持共用weightPtr，因为weightPtr内存在推理过程中是只读的。此处需注意，在共用weightPtr期间，不能释放weightPtr。 |
| weightSize | 输入 | 模型所需权值内存的大小，单位Byte。weightPtr为空指针时无效。 |

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

<!-- npu="IPV350" id4 -->
model参数处需申请Device上的内存。
<!-- end id4 -->

## 参考资源

当前还提供了[aclmdlSetConfigOpt](aclmdlSetConfigOpt.md)接口、[aclmdlLoadWithConfig](aclmdlLoadWithConfig.md)接口来实现模型加载，通过配置对象中的属性来区分，在加载模型时是从文件加载，还是从内存加载，以及内存是由系统内部管理，还是由用户管理。
