# ShardGraphsToFile

## 产品支持情况

- Ascend 950PR/Ascend 950DT：不支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 200I/500 A2 推理产品：不支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持
- MC62CM12A AI处理器：不支持
- BS9SX2A AI处理器：不支持
- BS9SX1A AI处理器：不支持
- IPV350：不支持

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

**该接口已废弃，请勿使用。**

**大模型分布式编译切分场景下使用**：Graph太大场景下，需要调用该接口，进行切分图，并将切分后的图保存为pb文件。

切分图接口，对Session内的图按照AddGraph的顺序进行切分，**切分后的图保存为pb文件**。

切分方式由[options参数说明](../options_params/options_parameters_description.md)中的ge.graphParallelOptionPath参数配置，若未开启并行切分功能，则接口不会进行切分。

切分后的图，命名规则：

- 若工作在SPMD模式，切分后的图的命名规则为：\{原图名\}\_\{ClusterId\}\_\{ItemId\}\_\{ChipId\}\_\{VirtualStageId\}\_\{原图GraphId\}，切分后的图的个数将为原图的个数\*切分份数。
- 若工作在非SPMD模式，切分后的图命名规则为：\{原图名\}\_\{原图GraphId\}，切分后的图的个数将等于原图个数。

切分图以后，原图将不在Session内存在，生成的新的切分后图的GraphId将使用其他ID替代原图的GraphId。

- **该接口与[SaveGraphsToPb](SaveGraphsToPb.md)接口区别：**

    ShardGraphsToFile适用于大模型分布式编译切分场景，而SaveGraphsToPb适用于任何图。

- **该接口与[ShardGraphs](ShardGraphs.md)接口区别**：
    - ShardGraphsToFile可以完成搜索策略、图的切分，并输出切分后的图等文件，**切分后的图也通过该接口落盘**（file\_path参数必须指定有效路径）。
    - 而ShardGraphs完成搜索策略、图的切分，并输出切分后的图等文件后，**切分后的图保存在内存中，然后通过[SaveGraphsToPb](SaveGraphsToPb.md)落盘**。

## 函数原型

```c++
Status ShardGraphsToFile(const char_t *file_path = "./") const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| file_path | 输入 | 保存图和权重的目录，必须为一个合法路径。<br>如果该参数为空，则只进行切分图，不生成pb文件。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功<br>FAILED：失败 |

## 约束说明

当前仅在并行切分模式开启时生效。

## 调用示例

```c++
Session session(options); // options中已开启并行切分功能
Graph init_graph("init_graph");
Graph first_graph("first_graph");
Graph second_graph("second_graph");
session.ShardGraphsToFile("/xxx/graph_path");
```
