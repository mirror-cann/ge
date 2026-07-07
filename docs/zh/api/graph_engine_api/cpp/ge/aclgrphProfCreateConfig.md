# aclgrphProfCreateConfig

## 产品支持情况

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphProfCreateConfig_res.md#id1 -->

## 头文件/库文件

- 头文件：\#include <ge/ge\_prof.h\>
- 库文件：libmsprofiler.so

## 功能说明

创建Profiling配置信息。

## 函数原型

```c++
aclgrphProfConfig *aclgrphProfCreateConfig(uint32_t *deviceid_list, uint32_t device_nums, ProfilingAicoreMetrics aicore_metrics, ProfAicoreEvents *aicore_events, uint64_t data_type_config)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| deviceid_list | 输入 | 需要采集数据的Device ID列表。 |
| device_nums | 输入 | Device个数。需要保证和deviceid_list中的Device数目一致。 |
| aicore_metrics | 输入 | AI Core metrics，枚举值请参见[ProfilingAicoreMetrics](ProfilingAicoreMetrics.md)。 |
| aicore_events | 输入 | AI Core events，预留项。 |
| data_type_config | 输入 | 指定需要采集的Profiling数据内容范围，具体请参见[ProfDataTypeConfig](ProfDataTypeConfig.md)。<br>数值可按bit位或的方式组合表征，指定输出多类数据信息。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| aclgrphProfConfig | aclgrphProfConfig * | Profiling配置信息结构体指针。 |

## 约束说明

- aclgrphProfConfig类型数据可以只创建一次，多处使用，用户需要保证数据的一致性和准确性。
- 与[aclgrphProfDestroyConfig](aclgrphProfDestroyConfig.md)接口配对使用，先调用aclgrphProfCreateConfig接口再调用aclgrphProfDestroyConfig接口。
- 用户需保证程序结束时调用aclgrphProfDestroyConfig销毁所有创建的Profiling配置信息，否则可能会导致内存泄漏。
- 如果用户想在不同的Device上指定不同的Profiling配置信息，则可创建不同的aclgrphProfConfig类型数据，并依次调用[aclgrphProfStart](aclgrphProfStart.md)接口将不同的配置信息下发到不同的Device上。同时注意Device信息不能有重复。
