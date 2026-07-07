# aclgrphParseONNXFromMem

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <parser/onnx\_parser.h\>
- 库文件：libfmk\_onnx\_parser.so

## 功能说明

将加载至内存的ONNX模型解析为Graph。

## 函数原型

```c++
graphStatus aclgrphParseONNXFromMem(const char *buffer, size_t size, const std::map<ge::AscendString, ge::AscendString> &parser_params, ge::Graph &graph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| buffer | 输入 | ONNX模型以二进制load到内存缓冲区的指针，尚未反序列化。 |
| size | 输入 | 内存缓冲区的大小。 |
| parser_params | 输入 | 配置参数map映射表，key为参数类型，value为参数值，均为AscendString格式，用于描述原始模型解析参数。<br>map中支持的配置参数请参见[Parser解析接口支持的配置参数](./parser_config_parameters.md)。 |
| graph | 输出 | 解析后生成的Graph。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
