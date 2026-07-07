# aclopDestroyHandle

## 产品支持情况

<!-- npu="950" id1084 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1084 -->
<!-- npu="A3" id1085 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1085 -->
<!-- npu="910b" id1086 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1086 -->
<!-- npu="310b" id1087 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1087 -->
<!-- npu="310p" id1088 -->
- Atlas 推理系列产品：支持
<!-- end id1088 -->
<!-- npu="910" id1089 -->
- Atlas 训练系列产品：支持
<!-- end id1089 -->
<!-- npu="IPV350" id1090 -->
- IPV350：不支持
<!-- end id1090 -->
- MC62CM12A AI处理器：不支持

## 功能说明

销毁通过[aclopCreateHandle](aclopCreateHandle.md)接口创建的执行算子的handle。

## 函数原型

```c
void aclopDestroyHandle(aclopHandle *handle)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| handle | 输入 | 待销毁的aclopHandle类型的指针。 |

## 返回值说明

无
