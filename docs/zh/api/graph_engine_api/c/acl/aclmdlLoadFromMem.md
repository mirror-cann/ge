# aclmdlLoadFromMem

## 产品支持情况

<!-- npu="950" id421 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id421 -->
<!-- npu="A3" id422 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id422 -->
<!-- npu="910b" id423 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id423 -->
<!-- npu="310b" id424 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id424 -->
<!-- npu="310p" id425 -->
- Atlas 推理系列产品：支持
<!-- end id425 -->
<!-- npu="910" id426 -->
- Atlas 训练系列产品：支持
<!-- end id426 -->
<!-- npu="IPV350" id427 -->
- IPV350：不支持
<!-- end id427 -->

## 功能说明

从内存加载om模型文件数据，由系统内部管理模型运行的内存。

关于如何获取om模型文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 \> 基础功能参数 \> 总体选项 \> --mode”。

## 函数原型

```c
aclError aclmdlLoadFromMem(const void *model, size_t modelSize, uint32_t *modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model | 输入 | 存放模型数据的内存地址指针。 |
| modelSize | 输入 | 内存中的模型数据长度，单位Byte。 |
| modelId | 输出 | 模型ID的指针。<br>系统成功加载模型后，返回模型ID作为后续操作时识别模型的标志。 |

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
