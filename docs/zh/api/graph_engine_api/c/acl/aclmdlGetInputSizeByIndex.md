# aclmdlGetInputSizeByIndex

## 产品支持情况

<!-- npu="950" id498 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id498 -->
<!-- npu="A3" id499 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id499 -->
<!-- npu="910b" id500 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id500 -->
<!-- npu="310b" id501 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id501 -->
<!-- npu="310p" id502 -->
- Atlas 推理系列产品：支持
<!-- end id502 -->
<!-- npu="910" id503 -->
- Atlas 训练系列产品：支持
<!-- end id503 -->
<!-- npu="IPV350" id504 -->
- IPV350：支持
<!-- end id504 -->

## 功能说明

根据模型描述信息获取指定输入的大小，单位为Byte。

## 函数原型

```c
size_t aclmdlGetInputSizeByIndex(aclmdlDesc *modelDesc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输入的大小，index值从0开始。 |

## 返回值说明

针对动态Batch、动态分辨率（宽高）的场景，返回最大档位对应的输入的大小；静态场景下，返回指定输入的大小。单位是Byte。

## 约束说明

如果模型输入的Shape是动态的、且维度的取值为-1（表示此维度可以使用\>=1的任意取值），则通过本接口获取的大小为0，用户需根据实际数据占用的内存大小来申请内存。
