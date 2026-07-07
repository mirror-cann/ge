# aclmdlLoadFromMemWithQ

## 产品支持情况

<!-- npu="950" id664 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id664 -->
<!-- npu="A3" id665 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id665 -->
<!-- npu="910b" id666 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id666 -->
<!-- npu="310b" id667 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id667 -->
<!-- npu="310p" id668 -->
- Atlas 推理系列产品：支持
<!-- end id668 -->
<!-- npu="910" id669 -->
- Atlas 训练系列产品：支持
<!-- end id669 -->
<!-- npu="IPV350" id670 -->
- IPV350：不支持
<!-- end id670 -->

## 功能说明

从内存加载om模型文件数据，模型的输入、输出数据都存放在队列中。本接口只支持加载固定Shape输入的模型。

关于如何获取om模型文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 \> 基础功能参数 \> 总体选项 \> --mode”。

## 函数原型

```c
aclError aclmdlLoadFromMemWithQ(const void *model, size_t modelSize, uint32_t *modelId, const uint32_t *inputQ, size_t inputQNum, const uint32_t *outputQ, size_t outputQNum)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model | 输入 | 存放模型数据的内存地址指针。 |
| modelSize | 输入 | 内存中的模型数据长度，单位Byte。 |
| modelId | 输出 | 模型ID的指针。<br>系统成功加载模型后，返回模型ID作为后续操作时识别模型的标志。 |
| inputQ | 输入 | 队列ID的指针，一个模型的输入对应一个队列ID。 |
| inputQNum | 输入 | 输入队列大小。 |
| outputQ | 输入 | 队列ID的指针，一个模型的输出对应一个队列ID。 |
| outputQNum | 输入 | 输出队列大小。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 参考资源

当前还提供了[aclmdlSetConfigOpt](aclmdlSetConfigOpt.md)接口、[aclmdlLoadWithConfig](aclmdlLoadWithConfig.md)接口来实现模型加载，通过配置对象中的属性来区分，在加载模型时是从文件加载，还是从内存加载，以及内存是由系统内部管理，还是由用户管理。
