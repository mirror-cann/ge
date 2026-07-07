# aclopDestroyAttr

## 产品支持情况

<!-- npu="950" id532 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id532 -->
<!-- npu="A3" id533 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id533 -->
<!-- npu="910b" id534 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id534 -->
<!-- npu="310b" id535 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id535 -->
<!-- npu="310p" id536 -->
- Atlas 推理系列产品：支持
<!-- end id536 -->
<!-- npu="910" id537 -->
- Atlas 训练系列产品：支持
<!-- end id537 -->
<!-- npu="IPV350" id538 -->
- IPV350：不支持
<!-- end id538 -->
- MC62CM12A AI处理器：不支持

## 功能说明

销毁通过[aclopCreateAttr](aclopCreateAttr.md)接口创建的aclopAttr类型的数据。

## 函数原型

```c
void aclopDestroyAttr(const aclopAttr *attr)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr | 输入 | 待销毁的aclopAttr类型指针。 |

## 返回值说明

无
