# aclDestroyGraphDumpOpt

## 产品支持情况

<!-- npu="950" id71 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id71 -->
<!-- npu="A3" id72 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id72 -->
<!-- npu="910b" id73 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id73 -->
<!-- npu="310b" id74 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id74 -->
<!-- npu="310p" id75 -->
- Atlas 推理系列产品：支持
<!-- end id75 -->
<!-- npu="910" id76 -->
- Atlas 训练系列产品：支持
<!-- end id76 -->
<!-- npu="IPV350" id77 -->
- IPV350：不支持
<!-- end id77 -->

## 功能说明

销毁通过[aclCreateGraphDumpOpt](aclCreateGraphDumpOpt.md)接口创建的aclGraphDumpOption类型的数据。

## 函数原型

```c
aclError aclDestroyGraphDumpOpt(const aclGraphDumpOption *graphDumpOpt)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graphDumpOpt | 输入 | 待销毁的aclGraphDumpOption类型的指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
