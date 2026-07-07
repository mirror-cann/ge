# AddGraphClone

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

向GeSession中添加Graph，GeSession内会生成唯一的Graph ID。

相比于[AddGraph](./AddGraph.md)接口，此接口传入Graph对象后，会产生Graph对象的拷贝。GeSession中保存的图是Graph对象的一个备份，后续对该Graph的修改不影响GeSession内原有Graph，同时GeSession内图的任何修改也不会影响Graph对象。

## 函数原型

```c++
Status AddGraphClone(uint32_t graph_id, const Graph &graph)
Status AddGraphClone(uint32_t graph_id, const Graph &graph, const std::map<AscendString, AscendString> &options)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| graph | 输入 | 需要加载到GeSession内的Graph。 |
| options | 输入 | map表，key为参数类型，value为参数值，均为字符串格式，描述Graph配置信息。<br>一般情况下可不填，与[GEInitializeV2](GEInitializeV2.md)传入的全局options保持一致。<br>如需单独配置当前Graph的配置信息时，可以通过此参数配置，支持的配置项范围和AddGraph一致。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_CLI_GE_NOT_INITIALIZED：GE未初始化。<br>SUCCESS：图添加成功。<br>FAILED：图添加失败。 |

## 约束说明

相同对象的Graph调用此接口注册，会导致不同的Graph ID对应不同的备份，两个不同Graph ID对应的备份不共享。
