# aclmdlLoadFromFile

## 产品支持情况

<!-- npu="950" id874 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id874 -->
<!-- npu="A3" id875 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id875 -->
<!-- npu="910b" id876 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id876 -->
<!-- npu="310b" id877 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id877 -->
<!-- npu="310p" id878 -->
- Atlas 推理系列产品：支持
<!-- end id878 -->
<!-- npu="910" id879 -->
- Atlas 训练系列产品：支持
<!-- end id879 -->
<!-- npu="IPV350" id880 -->
- IPV350：不支持
<!-- end id880 -->

## 功能说明

从文件加载离线模型数据，由系统内部管理模型运行的内存。

本接口中通过modelPath参数传入的文件是\*.om模型文件。关于如何获取om模型文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 \> 基础功能参数 \> 总体选项 \> --mode”。

若对om模型文件大小有限制，本接口还支持加载外置权重文件，但需在构建模型时，将权重保存在单独的文件中。例如在使用ATC工具生成om文件时，将--external\_weight参数设置为1（1表示将原始网络中的Const/Constant节点的权重保存在单独的文件中），且该文件保存在与om文件同级的weight目录下），那么在使用本接口加载om文件时，需将weight目录与om文件放在同级目录下，这时本接口会自行到weight目录下查找权重文件，否则可能会导致单独的权重文件加载不成功。

## 函数原型

```c
aclError aclmdlLoadFromFile(const char *modelPath, uint32_t *modelId)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelPath | 输入 | 模型文件路径的指针，路径中包含文件名。运行程序（APP）的用户需要对该存储路径有访问权限。 |
| modelId | 输出 | 模型ID的指针。<br>系统成功加载模型后，返回模型ID作为后续操作时识别模型的标志。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 参考资源

当前还提供了[aclmdlSetConfigOpt](aclmdlSetConfigOpt.md)接口、[aclmdlLoadWithConfig](aclmdlLoadWithConfig.md)接口来实现模型加载，通过配置对象中的属性来区分，在加载模型时是从文件加载，还是从内存加载，以及内存是由系统内部管理，还是由用户管理。
