# aclopUnregisterCompileFunc

## 产品支持情况

<!-- npu="950" id825 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id825 -->
<!-- npu="A3" id826 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id826 -->
<!-- npu="910b" id827 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id827 -->
<!-- npu="310b" id828 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id828 -->
<!-- npu="310p" id829 -->
- Atlas 推理系列产品：支持
<!-- end id829 -->
<!-- npu="910" id830 -->
- Atlas 训练系列产品：支持
<!-- end id830 -->
<!-- npu="IPV350" id831 -->
- IPV350：不支持
<!-- end id831 -->

## 功能说明

动态Shape场景下，取消注册算子选择器。

## 函数原型

```c
aclError aclopUnregisterCompileFunc(const char *opType)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 算子类型的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
