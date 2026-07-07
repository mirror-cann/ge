# aclopSetCompileFlag（废弃）

**须知：此接口后续版本会废弃，请使用[aclSetCompileopt](aclSetCompileopt.md)接口设置编译选项。**

## 产品支持情况

<!-- npu="950" id958 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id958 -->
<!-- npu="A3" id959 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id959 -->
<!-- npu="910b" id960 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id960 -->
<!-- npu="310b" id961 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id961 -->
<!-- npu="310p" id962 -->
- Atlas 推理系列产品：支持
<!-- end id962 -->
<!-- npu="910" id963 -->
- Atlas 训练系列产品：支持
<!-- end id963 -->
<!-- npu="IPV350" id964 -->
- IPV350：不支持
<!-- end id964 -->

## 功能说明

在编译算子前调用本接口设置算子编译标识，该设置进程级全局共享。

## 函数原型

```c
aclError aclopSetCompileFlag(aclOpCompileFlag flag)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| flag | 输入 | 用于设置算子编译标识。类型定义请参见[aclOpCompileFlag](aclOpCompileFlag.md)。<br>如果没有设置flag，则默认按照精确编译。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
