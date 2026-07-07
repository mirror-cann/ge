# aclmdlGetDescFromFile

## 产品支持情况

<!-- npu="950" id1210 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1210 -->
<!-- npu="A3" id1211 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1211 -->
<!-- npu="910b" id1212 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1212 -->
<!-- npu="310b" id1213 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1213 -->
<!-- npu="310p" id1214 -->
- Atlas 推理系列产品：支持
<!-- end id1214 -->
<!-- npu="910" id1215 -->
- Atlas 训练系列产品：支持
<!-- end id1215 -->
<!-- npu="IPV350" id1216 -->
- IPV350：支持
<!-- end id1216 -->

## 功能说明

根据模型文件获取该模型的模型描述信息。

通过本接口获取到的模型描述信息，无法应用于[aclmdlGetOpAttr](aclmdlGetOpAttr.md)、[aclmdlGetCurOutputDims](aclmdlGetCurOutputDims.md)接口。

## 函数原型

```c
aclError aclmdlGetDescFromFile(aclmdlDesc *modelDesc, const char *modelPath)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输出 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| modelPath | 输入 | 模型文件路径的指针，路径中包含文件名。运行程序（APP）的用户需要对该存储路径有访问权限。<br>此处的模型文件是om模型文件。关于如何获取om模型文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 > 基础功能参数 > 总体选项 > --mode”。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
