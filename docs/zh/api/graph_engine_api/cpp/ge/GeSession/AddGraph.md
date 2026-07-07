# AddGraph

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

向GeSession中添加Graph，GeSession内会生成唯一的Graph ID。

## 函数原型

```c++
Status AddGraph(uint32_t graph_id, const Graph &graph)
Status AddGraph(uint32_t graph_id, const Graph &graph, const std::map<AscendString, AscendString> &options)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| graph | 输入 | 需要加载到GeSession内的Graph。 |
| options | 输入 | map表，key为参数类型，value为参数值，均为字符串格式，描述Graph配置信息。<br>一般情况下可不填，与GEInitializeV2传入的全局options保持一致。<br>如需单独配置当前Graph的配置信息时，可以通过此参数配置，支持的配置项请参见[options参数说明](../options_params/options_parameters_description.md)中graph级别的参数。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_CLI_GE_NOT_INITIALIZED：GE未初始化。<br>SUCCESS：图添加成功。<br>FAILED：图添加失败。 |

## 约束说明

- 相同对象的Graph调用此接口注册，会导致不同的Graph ID实际共享同一个Graph对象，导致后续操作相互影响而出错。
- 不同的Graph对象请不要使用相同的Graph ID来添加，该情况下，只保留第一次添加的Graph对象，后续的Graph对象不会添加成功。
- 使用该接口，GeSession会直接修改添加的Graph对象。如果AddGraph后需要保持原有的Graph对象不受影响，应使用[AddGraphClone](AddGraphClone.md)接口，AddGraphClone会在Session中拷贝一份Graph对象，仅对Graph对象的拷贝进行修改。
