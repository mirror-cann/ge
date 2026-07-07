# GeSessionLoadGraph

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

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

只有异步执行Graph场景使用，使用指定的Session，将指定Graph ID的图绑定到对应Stream上。

GeSessionLoadGraph成功后可以使用[GeSessionExecuteGraphWithStreamAsync](GeSessionExecuteGraphWithStreamAsync.md)接口执行图。

## 函数原型

```c
ge::Status GeSessionLoadGraph(ge::Session &session, uint32_t graph_id, const std::map<ge::AscendString, ge::AscendString> &options, void *stream)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| session | 输入 | 加载图的Session实例。 |
| graph_id | 输入 | 要执行图对应的ID。 |
| options | 输入 | 执行阶段可能用到的options。map表，key为参数类型，value为参数值，描述Graph配置信息。<br>一般情况下可不填，与GEInitialize传入的全局options保持一致。<br>key和value类型为AscendString，如需单独配置当前Graph的配置信息时，可以通过此参数配置，支持的配置项请参见[options参数说明](../../cpp/ge/options_params/options_parameters_description.md)>基础功能>ge.exec.frozenInputIndexes，当前只支持配置该参数。 |
| stream | 输入 | 图执行流。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_CLI_SESS_RUN_FAILED：执行子图时序列化转换失败。<br>SUCCESS：执行子图成功。<br>FAILED：执行子图失败。 |

## 约束说明

该接口执行前需要完成[CompileGraph](../../cpp/ge/Session/CompileGraph.md)流程，且需要与[GeSessionExecuteGraphWithStreamAsync](GeSessionExecuteGraphWithStreamAsync.md)接口配合使用。
