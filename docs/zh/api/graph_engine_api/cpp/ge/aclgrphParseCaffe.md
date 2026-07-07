# aclgrphParseCaffe

## 产品支持情况

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphParseCaffe_res.md#id1 -->

## 头文件/库文件

- 头文件：\#include <parser/caffe\_parser.h\>
- 库文件：libfmk\_parser.so

## 功能说明

将Caffe模型解析为图。

## 函数原型

```c++
graphStatus aclgrphParseCaffe(const char *model_file, const char *weights_file, ge::Graph &graph)
graphStatus aclgrphParseCaffe(const char *model_file, const char *weights_file, const std::map<ge::AscendString, ge::AscendString> &parser_params, ge::Graph &graph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model_file | 输入 | Caffe原始模型文件路径。 |
| weights_file | 输入 | Caffe权重文件路径。 |
| parser_params | 输入 | 配置参数map映射表，key为参数类型，value为参数值，均为AscendString格式，用于描述原始模型解析参数。<br>map中支持的配置参数请参见[Parser解析接口支持的配置参数](./parser_config_parameters.md)。 |
| graph | 输出 | 解析后生成的图。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
