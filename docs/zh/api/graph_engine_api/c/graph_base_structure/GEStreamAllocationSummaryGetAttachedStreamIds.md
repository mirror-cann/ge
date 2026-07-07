# GEStreamAllocationSummaryGetAttachedStreamIds

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 200I/500 A2 推理产品：不支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持
- MC62CM12A AI处理器：不支持
- BS9SX2A AI处理器：不支持
- BS9SX1A AI处理器：不支持
- IPV350：不支持

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取根图和子图附属逻辑从流ID。

## 函数原型

```c
ge::Status GEStreamAllocationSummaryGetAttachedStreamIds(const ge::CompiledGraphSummary &compiled_graph_summary, std::map<AscendString, std::vector<std::vector<int64_t>>> &graph_to_attached_stream_ids)
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| compiled_graph_summary | 输入 | 图编译后的概要信息。 |
| graph_to_attached_stream_ids | 输出 | map格式，key为图名称，value为附属逻辑从流ID的向量，其中索引表示逻辑流ID。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | ge::Status | - SUCCESS：接口调用成功。<br>  - FAILED：接口调用失败 |

## 约束说明

无
