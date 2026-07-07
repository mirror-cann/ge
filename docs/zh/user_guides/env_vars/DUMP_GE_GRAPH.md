# DUMP\_GE\_GRAPH

## 功能描述

把整个流程中各个阶段的图描述信息打印到文件中，此环境变量控制dump图的内容多少。

支持的取值包括：

- 1：包含连边关系和数据信息的完整版dump。
- 2：不含有权重等数据的基本版dump。
- 3：只显示节点关系的精简版dump。

设置该环境变量后，在当前路径会生成如下文件：

- ge\_onnx\*.pbtxt：基于ONNX的模型描述结构，可以使用Netron等可视化软件打开。
- ge\_proto\*.txt：protobuf格式存储的文本文件，该文件可以转成JSON格式文件方便用户定位问题。该文件与ge\_onnx\*.pbtxt一般成对出现，但是ge\_proto\*.txt比ge\_onnx\*.pbtxt文件会多string类型的属性信息，因此ge\_proto\*.txt显示得更完整，用户选择其中一种文件打开。

    由于ge\_proto\*.txt文件结构相比ge\_onnx\*.pbtxt已经做了文件大小的优化，因此DUMP\_GE\_GRAPH环境变量设置为2或3，对ge\_proto\*.txt文件效果相同，都显示为不含有权重等数据的基本版dump。

- ge\_readable\*.txt：（可选）类似Dynamo fx图风格的高可读性文本文件。该类型的文件只在设置了DUMP\_GRAPH\_FORMAT环境变量，且取值中包括“readable”才会生成。

## 配置示例

```bash
export DUMP_GE_GRAPH=1
```

## 使用约束

无

## 支持的型号

全量芯片支持
