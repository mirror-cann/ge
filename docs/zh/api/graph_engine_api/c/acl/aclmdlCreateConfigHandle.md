# aclmdlCreateConfigHandle

## 产品支持情况

<!-- npu="950" id274 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id274 -->
<!-- npu="A3" id275 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id275 -->
<!-- npu="910b" id276 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id276 -->
<!-- npu="310b" id277 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id277 -->
<!-- npu="310p" id278 -->
- Atlas 推理系列产品：支持
<!-- end id278 -->
<!-- npu="910" id279 -->
- Atlas 训练系列产品：支持
<!-- end id279 -->
<!-- npu="IPV350" id280 -->
- IPV350：支持
<!-- end id280 -->

## 功能说明

创建aclmdlConfigHandle类型的数据，表示一个模型加载的配置对象。

如需销毁aclmdlConfigHandle类型的数据，请参见[aclmdlDestroyConfigHandle](aclmdlDestroyConfigHandle.md)。

## 函数原型

```c
aclmdlConfigHandle *aclmdlCreateConfigHandle()
```

## 参数说明

无

## 返回值说明

- 返回aclmdlConfigHandle类型的指针，表示成功。
- 返回nullptr，表示失败。
