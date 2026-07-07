# aclmdlQueryExeOMDesc

## 产品支持情况

<!-- npu="950" id246 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id246 -->
<!-- npu="A3" id247 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id247 -->
<!-- npu="910b" id248 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id248 -->
<!-- npu="310b" id249 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id249 -->
<!-- npu="310p" id250 -->
- Atlas 推理系列产品：不支持
<!-- end id250 -->
<!-- npu="910" id251 -->
- Atlas 训练系列产品：不支持
<!-- end id251 -->
<!-- npu="IPV350" id252 -->
- IPV350：支持
<!-- end id252 -->

## 功能说明

根据模型文件获取模型执行时所需的工作内存、权值内存、模型描述信息、静态和动态shape任务等的内存大小。

<!-- npu="IPV350" id1 -->
此处的模型文件是exeom模型文件。关于如何获取exeom文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 \> 基础功能参数 \> 总体选项 \> --mode”。
<!-- end id1 -->

## 函数原型

```c
aclError aclmdlQueryExeOMDesc(const char *fileName, [aclmdlExeOMDesc](aclmdlExeOMDesc.md) *mdlPartitionSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| fileName | 输入 | 模型文件路径的指针，路径中包含文件名。运行程序（APP）的用户需要对该路径有访问权限。 |
| mdlPartitionSize | 输出 | 模型执行时所需的各分区大小的结构体指针，分区大小单位为Byte。类型定义请参见[aclmdlExeOMDesc](aclmdlExeOMDesc.md)。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
