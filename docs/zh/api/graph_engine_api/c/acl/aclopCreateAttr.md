# aclopCreateAttr

## 产品支持情况

<!-- npu="950" id1049 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1049 -->
<!-- npu="A3" id1050 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1050 -->
<!-- npu="910b" id1051 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1051 -->
<!-- npu="310b" id1052 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1052 -->
<!-- npu="310p" id1053 -->
- Atlas 推理系列产品：支持
<!-- end id1053 -->
<!-- npu="910" id1054 -->
- Atlas 训练系列产品：支持
<!-- end id1054 -->
<!-- npu="IPV350" id1055 -->
- IPV350：不支持
<!-- end id1055 -->
- MC62CM12A AI处理器：不支持

## 功能说明

创建aclopAttr类型的数据，该数据类型用于保存算子的属性。

如需销毁aclopAttr类型的数据，请参见[aclopDestroyAttr](aclopDestroyAttr.md)。

## 函数原型

```c
aclopAttr *aclopCreateAttr()
```

## 参数说明

无

## 返回值说明

返回aclopAttr类型的指针。
