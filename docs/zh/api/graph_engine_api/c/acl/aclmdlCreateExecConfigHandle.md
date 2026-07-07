# aclmdlCreateExecConfigHandle

## 产品支持情况

<!-- npu="950" id365 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id365 -->
<!-- npu="A3" id366 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id366 -->
<!-- npu="910b" id367 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id367 -->
<!-- npu="310b" id368 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id368 -->
<!-- npu="310p" id369 -->
- Atlas 推理系列产品：支持
<!-- end id369 -->
<!-- npu="910" id370 -->
- Atlas 训练系列产品：支持
<!-- end id370 -->
<!-- npu="IPV350" id371 -->
- IPV350：支持
<!-- end id371 -->

## 功能说明

创建aclmdlExecConfigHandle类型的数据，表示一个模型执行的配置对象。

如需销毁aclmdlExecConfigHandle类型的数据，请参见[aclmdlDestroyExecConfigHandle](aclmdlDestroyExecConfigHandle.md)。

## 函数原型

```c
aclmdlExecConfigHandle *aclmdlCreateExecConfigHandle()
```

## 参数说明

无

## 返回值说明

- 返回aclmdlExecConfigHandle类型的指针，表示成功。
- 返回NULL，表示失败。
