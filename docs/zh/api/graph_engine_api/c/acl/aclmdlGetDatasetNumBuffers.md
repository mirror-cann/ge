# aclmdlGetDatasetNumBuffers

## 产品支持情况

<!-- npu="950" id713 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id713 -->
<!-- npu="A3" id714 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id714 -->
<!-- npu="910b" id715 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id715 -->
<!-- npu="310b" id716 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id716 -->
<!-- npu="310p" id717 -->
- Atlas 推理系列产品：支持
<!-- end id717 -->
<!-- npu="910" id718 -->
- Atlas 训练系列产品：支持
<!-- end id718 -->
<!-- npu="IPV350" id719 -->
- IPV350：支持
<!-- end id719 -->

## 功能说明

获取aclmdlDataset中aclDataBuffer的个数。

## 函数原型

```c
size_t aclmdlGetDatasetNumBuffers(const aclmdlDataset *dataset)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| dataset | 输入 | aclmdlDataset类型的指针。<br>需提前调用[aclmdlCreateDataset](aclmdlCreateDataset.md)接口创建aclmdlDataset类型的数据。 |

## 返回值说明

aclDataBuffer的个数。
