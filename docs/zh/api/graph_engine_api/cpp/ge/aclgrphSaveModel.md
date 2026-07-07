# aclgrphSaveModel

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

将离线模型序列化并保存到指定文件中。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus aclgrphSaveModel(const std::string &output_file, const ModelBufferData &model)
graphStatus aclgrphSaveModel(const char_t *output_file, const ModelBufferData &model)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model | 输入 | 离线模型缓冲数据。详情请参见[ModelBufferData](ModelBufferData.md)。<br>其中data指向生成的模型数据，length代表该模型的实际大小。 |
| output_file | 输入 | 保存离线模型的文件名。生成的离线模型文件名会自动以.om后缀结尾，例如ir_build_sample.om或者<br>ir_build_sample_linux_x86_64.om，若om文件名中包含操作系统及架构，则该om文件只能在该操作系统及架构的运行环境中使用。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

若生成的om模型文件名中含操作系统及架构，但操作系统及其架构与模型运行环境不一致时，需要与OPTION\_HOST\_ENV\_OS、OPTION\_HOST\_ENV\_CPU参数配合使用，设置模型运行环境的操作系统类型及架构。参数具体介绍请参见[aclgrphBuildInitialize支持的配置参数](./aclgrphBuildInitialize_config_params/basic_functions.md)。
