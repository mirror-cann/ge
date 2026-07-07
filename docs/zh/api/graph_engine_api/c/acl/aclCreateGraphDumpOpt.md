# aclCreateGraphDumpOpt

## 产品支持情况

<!-- npu="950" id323 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id323 -->
<!-- npu="A3" id324 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id324 -->
<!-- npu="910b" id325 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id325 -->
<!-- npu="310b" id326 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id326 -->
<!-- npu="310p" id327 -->
- Atlas 推理系列产品：支持
<!-- end id327 -->
<!-- npu="910" id328 -->
- Atlas 训练系列产品：支持
<!-- end id328 -->
<!-- npu="IPV350" id329 -->
- IPV350：不支持
<!-- end id329 -->

## 功能说明

创建aclGraphDumpOption类型的数据，表示单算子图Dump配置信息，用于设置图Dump时的选项（当前还没有选项，预留该数据类型，便于后续扩展）。

## 函数原型

```c
aclGraphDumpOption *aclCreateGraphDumpOpt()
```

## 参数说明

无

## 返回值说明

- 返回aclGraphDumpOption类型的指针，表示成功。
- 返回nullptr，表示失败。
