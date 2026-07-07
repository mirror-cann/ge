# aclgrphBuildModel

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

将输入的Graph编译为适配AI处理器的离线模型，并保存到内存缓冲区。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus aclgrphBuildModel(const ge::Graph &graph, const std::map<std::string, std::string> &build_options, ModelBufferData &model)
graphStatus aclgrphBuildModel(const ge::Graph &graph, const std::map<AscendString, AscendString> &build_options, ModelBufferData &model)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 待编译的Graph。 |
| build_options | 输入 | graph级别配置参数。<br>配置参数map映射表，key为参数类型，value为参数值，均为字符串格式，用于描述离线模型编译配置信息。<br>map中的配置参数请参见[aclgrphBuildModel支持的配置参数](./aclgrphBuildModel_config_params/aclgrphbuildmodel_config_params.md)。 |
| model | 输出 | 编译生成的离线模型缓存，模型保存在内存缓冲区中。详情请参见[ModelBufferData](ModelBufferData.md)。<br>其中data指向生成的模型数据，length代表该模型的实际大小。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

- **对于aclgrphBuildModel和aclgrphBuildInitialize中重复的编译配置参数，建议不要重复配置，如果设置重复，则以aclgrphBuildModel传入的为准**。
- 使用aclgrphBuildModel接口传入build\_options参数时，多张图场景下，如果传入的参数为ge::ir\_option::PRECISION\_MODE或者ge::ir\_option::PRECISION\_MODE\_V2，多张图设置的参数值需要相同。
- 使用aclgrphBuildModel接口编译的离线模型，保存在内存缓冲区中：

    - 如果希望将内存缓冲区中的模型保存为离线模型文件xx.om，则需要调用[aclgrphSaveModel](aclgrphSaveModel.md)接口，序列化保存离线模型到文件中。后续进行推理业务，需要使用**从文件中**加载模型的接口，例如aclmdlLoadFromFile，然后使用aclmdlExecute接口执行推理。
    - 如果离线模型保存在内存缓冲区：

        后续进行推理业务时，需要使用**从内存中**加载模型的接口，例如aclmdlLoadFromMem，然后使用aclmdlExecute接口执行推理。

    接口详细使用说明请参见《应用开发 \(C&C++\)》中的“模型推理”章节。
