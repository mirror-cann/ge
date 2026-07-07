# aclmdlAddDatasetBuffer

## 产品支持情况

<!-- npu="950" id183 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id183 -->
<!-- npu="A3" id184 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id184 -->
<!-- npu="910b" id185 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id185 -->
<!-- npu="310b" id186 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id186 -->
<!-- npu="310p" id187 -->
- Atlas 推理系列产品：支持
<!-- end id187 -->
<!-- npu="910" id188 -->
- Atlas 训练系列产品：支持
<!-- end id188 -->
<!-- npu="IPV350" id189 -->
- IPV350：支持
<!-- end id189 -->

## 功能说明

向aclmdlDataset中增加aclDataBuffer。

## 函数原型

```c
aclError aclmdlAddDatasetBuffer(aclmdlDataset *dataset, aclDataBuffer *dataBuffer)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| dataset | 输出 | 待增加aclDataBuffer的aclmdlDataset地址指针。<br>需提前调用[aclmdlCreateDataset](aclmdlCreateDataset.md)接口创建aclmdlDataset类型的数据。 |
| dataBuffer | 输入 | 待增加的aclDataBuffer地址指针。<br>需提前调用aclCreateDataBuffer接口创建aclDataBuffer类型的数据。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
