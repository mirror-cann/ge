# aclopSetMaxOpQueueNum

## 产品支持情况

<!-- npu="950" id776 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id776 -->
<!-- npu="A3" id777 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id777 -->
<!-- npu="910b" id778 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id778 -->
<!-- npu="310b" id779 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id779 -->
<!-- npu="310p" id780 -->
- Atlas 推理系列产品：支持
<!-- end id780 -->
<!-- npu="910" id781 -->
- Atlas 训练系列产品：支持
<!-- end id781 -->
<!-- npu="IPV350" id782 -->
- IPV350：不支持
<!-- end id782 -->

## 功能说明

通过单算子模型方式执行单个算子时，配置算子缓存信息老化，以达到节约内存和平衡调用性能的目的。

## 函数原型

```c
aclError aclopSetMaxOpQueueNum(uint64_t maxOpNum)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| maxOpNum | 输入 | “算子类型-单算子模型”映射队列的最大长度。<br>如果长度达到最大，则会先删除长期未使用的映射信息以及缓存中的单算子模型，再加载最新的映射信息以及对应的单算子模型。<br>通过单算子模型方式执行单个算子时（aclopUpdateParams接口执行单算子除外），如果不调用本接口配置映射队列的最大长度，则默认最大长度为20000。<br>aclopUpdateParams接口执行单算子时，如果不调用本接口配置映射队列的最大长度，则表示无需老化算子缓存信息。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
