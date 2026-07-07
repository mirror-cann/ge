# aclGetCompileoptSize

## 产品支持情况

<!-- npu="950" id804 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id804 -->
<!-- npu="A3" id805 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id805 -->
<!-- npu="910b" id806 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id806 -->
<!-- npu="310b" id807 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id807 -->
<!-- npu="310p" id808 -->
- Atlas 推理系列产品：支持
<!-- end id808 -->
<!-- npu="910" id809 -->
- Atlas 训练系列产品：支持
<!-- end id809 -->
<!-- npu="IPV350" id810 -->
- IPV350：不支持
<!-- end id810 -->

## 功能说明

用户可调用本接口获取存放编译选项值的内存大小。

用户在调用[aclGetCompileopt](aclGetCompileopt.md)前需要申请一块内存来保存编译选项的值，内存大小可以通过调用本接口获得。

## 函数原型

```c
size_t aclGetCompileoptSize(aclCompileOpt opt)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opt | 输入 | 编译选项。类型定义请参见[aclCompileOpt](aclCompileOpt.md)。 |

## 返回值说明

返回存放该编译选项值的内存大小，单位：Byte。
