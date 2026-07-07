# LoadGraph

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

异步执行Graph场景使用，将指定Graph ID的图绑定到对应Stream上。

该接口执行前需要完成[CompileGraph](CompileGraph.md)流程，且LoadGraph成功后需要使用[ExecuteGraphWithStreamAsync](ExecuteGraphWithStreamAsync.md)接口执行图。

## 函数原型

```c++
Status LoadGraph(const uint32_t graph_id, const std::map<AscendString, AscendString> &options, void *stream) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | 要执行Graph对应的ID。 |
| options | 输入 | 执行阶段可能用到的options。map表，key为参数类型，value为参数值，描述Graph配置信息。<br>一般情况下可不填，与[GEInitialize](GEInitialize.md)传入的全局options保持一致。<br>key和value类型为AscendString，如需单独配置当前Graph的配置信息时，可以通过此参数配置，支持的配置项请参见[options参数说明](../options_params/options_parameters_description.md)>基础功能>ge.exec.frozenInputIndexes和ge.exec.hostInputIndexes，当前只支持配置上述两个参数。 |
| stream（异步执行Graph场景） | 输入 | Graph执行流。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_CLI_SESS_RUN_FAILED：图执行时序列化转换失败。<br>SUCCESS：图执行成功。<br>FAILED：图执行失败。 |

## 约束说明

通过LoadGraph加载的Stream，和通过[ExecuteGraphWithStreamAsync](ExecuteGraphWithStreamAsync.md)接口运行时使用的Stream，推荐是同一条Stream，如果不是同一条Stream，需要在LoadGraph后，对加载使用的Stream调用流同步接口“aclrtSynchronizeStream”完成同步。

接口详细说明请参见《Runtime运行时 API》中的“Stream管理”  。
