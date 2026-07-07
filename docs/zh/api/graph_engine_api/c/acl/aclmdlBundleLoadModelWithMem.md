# aclmdlBundleLoadModelWithMem

## 产品支持情况

<!-- npu="950" id671 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id671 -->
<!-- npu="A3" id672 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id672 -->
<!-- npu="910b" id673 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id673 -->
<!-- npu="310b" id674 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id674 -->
<!-- npu="310p" id675 -->
- Atlas 推理系列产品：支持
<!-- end id675 -->
<!-- npu="910" id676 -->
- Atlas 训练系列产品：支持
<!-- end id676 -->
<!-- npu="IPV350" id677 -->
- IPV350：不支持
<!-- end id677 -->

## 功能说明

根据图索引加载模型中的图，由用户自行管理模型运行的内存。

## 函数原型

```c
aclError aclmdlBundleLoadModelWithMem(uint32_t bundleId, size_t index, void *workPtr, size_t workSize, void *weightPtr, size_t weightSize, uint32_t *modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| bundleId | 输入 | 通过[aclmdlBundleInitFromFile](aclmdlBundleInitFromFile.md)或者[aclmdlBundleInitFromMem](aclmdlBundleInitFromMem.md)接口初始化模型成功后返回的bundleId。 |
| index | 输入 | 索引。<br>用户调用[aclmdlBundleGetQueryModelNum](aclmdlBundleGetQueryModelNum.md)接口获取图总数后，这个index的取值范围：[0, (图总数-1)]。 |
| workPtr | 输入 | Device上模型所需工作内存（存放模型执行过程中的临时数据）的地址指针，由用户自行管理，模型执行过程中不能释放该内存。<br>如果在workPtr参数处传入空指针，表示由系统管理内存。<br>由用户自行管理工作内存时，如果多个模型串行执行，支持共用同一个工作内存，但用户需确保模型的串行执行顺序、且工作内存的大小需按多个模型中最大工作内存的大小来申请，例如通过以下方式保证串行：<br>  - 同步模型执行时，加锁，保证执行任务串行。<br>  - 异步模型执行时，使用同一个stream，保证执行任务串行。 |
| workSize | 输入 | 模型所需工作内存的大小，单位Byte。workPtr为空指针时无效。 |
| weightPtr | 输入 | Device上模型权值内存（存放权值数据）的地址指针，由用户自行管理，模型执行过程中不能释放该内存。<br>如果在weightPtr参数处传入空指针，表示由系统管理内存。<br>由用户自行管理权值内存时，在多线程场景下，对于同一个模型，如果在每个线程中都加载了一次，支持共用weightPtr，因为weightPtr内存在推理过程中是只读的。此处需注意，在共用weightPtr期间，不能释放weightPtr。 |
| weightSize | 输入 | 模型所需权值内存的大小，单位Byte。weightPtr为空指针时无效。 |
| modelId | 输出 | 实际可执行的图ID。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
