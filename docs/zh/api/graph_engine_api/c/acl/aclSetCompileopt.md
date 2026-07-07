# aclSetCompileopt

## 产品支持情况

<!-- npu="950" id393 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id393 -->
<!-- npu="A3" id394 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id394 -->
<!-- npu="910b" id395 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id395 -->
<!-- npu="310b" id396 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id396 -->
<!-- npu="310p" id397 -->
- Atlas 推理系列产品：支持
<!-- end id397 -->
<!-- npu="910" id398 -->
- Atlas 训练系列产品：支持
<!-- end id398 -->
<!-- npu="IPV350" id399 -->
- IPV350：不支持
<!-- end id399 -->

## 功能说明

用户可调用本接口设置对应的编译选项，该选项为进程级全局共享。在算子或模型编译前，调用本接口设置编译选项，在编译时，以当前的编译选项为准，一次编译过程中不会变更。

## 函数原型

```c
aclError aclSetCompileopt(aclCompileOpt opt, const char *value)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opt | 输入 | 编译选项。类型定义请参见[aclCompileOpt](aclCompileOpt.md)。 |
| value | 输入 | 编译选项具体值的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
