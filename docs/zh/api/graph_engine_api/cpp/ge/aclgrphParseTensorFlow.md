# aclgrphParseTensorFlow

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <parser/tensorflow\_parser.h\>
- 库文件：libfmk\_parser.so

## 功能说明

将TensorFlow模型解析为Graph。

## 函数原型

```c++
graphStatus aclgrphParseTensorFlow(const char *model_file, ge::Graph &graph)
graphStatus aclgrphParseTensorFlow(const char *model_file, const std::map<ge::AscendString, ge::AscendString> &parser_params, ge::Graph &graph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model_file | 输入 | TensorFlow原始模型文件路径。 |
| parser_params | 输入 | 配置参数map映射表，key为参数类型，value为参数值，均为AscendString格式，用于描述原始模型解析参数。<br>map中支持的配置参数请参见[Parser解析接口支持的配置参数](./parser_config_parameters.md)。 |
| graph | 输出 | 解析后生成的Graph。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

使用该接口解析的Graph，Graph名中会包含时间戳，因此多次调用该接口，Graph名会不同。
