# aclgrphBuildInitialize

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

模型构建的初始化函数，用于申请资源。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus aclgrphBuildInitialize(std::map<std::string, std::string> global_options)
graphStatus aclgrphBuildInitialize(std::map<AscendString, AscendString> &global_options)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| global_options | 输入 | 全局配置参数。<br>配置参数map映射表，key为参数类型，value为参数值，均为字符串格式，用于描述离线模型编译初始化信息。<br>map中支持的配置参数请参见[aclgrphBuildInitialize支持的配置参数](./aclgrphBuildInitialize_config_params/aclgrphbuildinitialize_config_params.md)。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

一个进程内只能调用一次aclgrphBuildInitialize接口。

调用该接口后，可以在同一进程中连续调用多次[aclgrphBuildModel](aclgrphBuildModel.md)接口。

**对于aclgrphBuildModel和aclgrphBuildInitialize中重复的options编译配置参数，建议不要重复配置，如果设置重复，则以aclgrphBuildModel传入的为准**。
