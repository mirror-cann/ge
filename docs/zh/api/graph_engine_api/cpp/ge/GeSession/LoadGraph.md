# LoadGraph

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

加载图并为其执行做准备，包括申请与管理图运行所需的内存及计算流等资源。

说明：若在调用本接口前未执行[CompileGraph](./CompileGraph.md)完成图编译，则本接口将自动调用CompileGraph以完成编译。

## 函数原型

```c++
Status LoadGraph(const uint32_t graph_id, const std::map<AscendString, AscendString> &options, void *stream) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | 要执行Graph对应的ID。 |
| options | 输入 | 执行阶段可能用到的options。map表，key为参数类型，value为参数值，描述Graph配置信息。<br>一般情况下可不填，与[GEInitializeV2](GEInitializeV2.md)传入的全局options保持一致。<br>key和value类型为AscendString，如需单独配置当前Graph的配置信息时，可以通过此参数配置，支持的配置项请参见[options参数说明](../options_params/basic_functions.md)>基础功能>ge.exec.frozenInputIndexes和ge.exec.hostInputIndexes，当前只支持配置上述两个参数。 |
| stream | 输入 | 接口“aclrtCreateStream”创建的流，也可以设置为nullptr。当传入有效值时，若在加载过程中需要向流中下发任务，会下发到指定流上。<br><br>  - 若与[RunGraphWithStreamAsync](./RunGraphWithStreamAsync.md)接口配合使用，建议传入有效值。此时，通过LoadGraph加载的Stream与RunGraphWithStreamAsync运行时使用的Stream推荐为同一条流。若非同一条流，则需在LoadGraph后，对加载使用的Stream调用流同步接口“aclrtSynchronizeStream”完成同步。<br>  - 若与[RunGraph](./RunGraph.md)或[RunGraphAsync](./RunGraphAsync.md)接口配合使用，建议传入nullptr以简化流程。若传入有效值，则需在LoadGraph后、RunGraph/RunGraphAsync前调用“aclrtSynchronizeStream”完成流同步，以确保加载任务完成。<br><br>接口详细说明请参见《Runtime运行时 API》中的“Stream管理”。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_CLI_SESS_RUN_FAILED：图执行时序列化转换失败。<br>SUCCESS：图执行成功。<br>FAILED：图执行失败。 |
