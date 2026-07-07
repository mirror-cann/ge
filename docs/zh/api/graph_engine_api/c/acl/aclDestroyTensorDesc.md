# aclDestroyTensorDesc

## 产品支持情况

<!-- npu="950" id288 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id288 -->
<!-- npu="A3" id289 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id289 -->
<!-- npu="910b" id290 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id290 -->
<!-- npu="310b" id291 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id291 -->
<!-- npu="310p" id292 -->
- Atlas 推理系列产品：支持
<!-- end id292 -->
<!-- npu="910" id293 -->
- Atlas 训练系列产品：支持
<!-- end id293 -->
<!-- npu="IPV350" id294 -->
- IPV350：不支持
<!-- end id294 -->

## 功能说明

销毁aclTensorDesc类型的数据。

## 函数原型

```c
void aclDestroyTensorDesc(const aclTensorDesc *desc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输入 | 待销毁的[aclTensorDesc](aclTensorDesc.md)类型的指针。 |

## 返回值说明

无
