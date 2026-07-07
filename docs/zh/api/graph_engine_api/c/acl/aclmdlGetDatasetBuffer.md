# aclmdlGetDatasetBuffer

## 产品支持情况

<!-- npu="950" id1189 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1189 -->
<!-- npu="A3" id1190 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1190 -->
<!-- npu="910b" id1191 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1191 -->
<!-- npu="310b" id1192 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1192 -->
<!-- npu="310p" id1193 -->
- Atlas 推理系列产品：支持
<!-- end id1193 -->
<!-- npu="910" id1194 -->
- Atlas 训练系列产品：支持
<!-- end id1194 -->
<!-- npu="IPV350" id1195 -->
- IPV350：支持
<!-- end id1195 -->

## 功能说明

获取aclmdlDataset中的第n个aclDataBuffer。

## 函数原型

```c
aclDataBuffer* aclmdlGetDatasetBuffer(const aclmdlDataset *dataset, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| dataset | 输入 | aclmdlDataset类型的指针。<br>需提前调用[aclmdlCreateDataset](aclmdlCreateDataset.md)接口创建aclmdlDataset类型的数据。 |
| index | 输入 | 表明获取的是第几个aclDataBuffer。 |

## 返回值说明

- 获取成功，返回aclDataBuffer的地址。
- 获取失败返回空地址。
