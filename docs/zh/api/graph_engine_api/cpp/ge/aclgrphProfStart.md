# aclgrphProfStart

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
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphProfStart_res.md#id1 -->

## 头文件/库文件

- 头文件：\#include <ge/ge\_prof.h\>
- 库文件：libmsprofiler.so

## 功能说明

下发Profiling请求，开启对应数据的采集。

## 函数原型

```c++
Status aclgrphProfStart(aclgrphProfConfig *profiler_config)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| profiler_config | 输入 | Profiling配置信息结构指针。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功<br>其他：失败 |

## 约束说明

- 该接口在[RunGraph](./Session/RunGraph.md)之前调用，若在模型执行过程中调用，Profiling采集到的数据为调用aclgrphProfStart接口之后的数据，可能导致数据不完整。
- 该接口和aclgrphProfStop配对使用，先调用aclgrphProfStart接口再调用aclgrphProfStop接口。
- aclgrphProfInit \> aclgrphProfStart \> aclgrphProfStop \> aclgrphProfFinalize为一条完整的接口调用流程，如果用户想要单进程内切换模型或图，做多轮执行的时候，也需要按照上述完整的流程执行，不支持某个接口的无序调用或并排多次调用。
