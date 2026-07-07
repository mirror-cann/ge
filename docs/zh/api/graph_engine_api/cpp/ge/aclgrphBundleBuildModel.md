# aclgrphBundleBuildModel

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

将输入的一组Graph编译为适配AI处理器的离线模型。

与[aclgrphBuildModel](aclgrphBuildModel.md)接口的区别是，该接口适用于权重更新场景。通过aclgrphBundleBuildModel接口生成离线模型缓存后，需要使用[aclgrphBundleSaveModel](aclgrphBundleSaveModel.md)接口落盘。

## 函数原型

```c++
graphStatus aclgrphBundleBuildModel(const std::vector<ge::GraphWithOptions> &graph_with_options, ModelBufferData &model)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_with_options | 输入 | 待编译的一组图和编译参数。该入参为一个结构体，包括参数请参见下面的GraphWithOptions结构体。<br>一组图包括：权重初始化图，权重更新图，推理图，其中只有推理图支持设置如下options参数。<br>可以通过传入options参数配置离线模型编译配置信息，当前支持的配置参数请参见[aclgrphBuildModel支持的配置参数](./aclgrphBuildModel_config_params/aclgrphbuildmodel_config_params.md)。 |
| model | 输出 | 编译生成的离线模型缓存。详情请参见[ModelBufferData](ModelBufferData.md)。<br>其中data指向生成的模型数据，length代表该模型的实际大小。 |

```c++
struct GraphWithOptions {
  ge::Graph graph;
  std::map<AscendString, AscendString> build_options;
};
```

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

该接口内多个图如果涉及共享同名variable算子，接口内部会进行variable算子加速融合，建议通过[aclgrphConvertToWeightRefreshableGraphs](aclgrphConvertToWeightRefreshableGraphs.md)接口生成，否则可能会出现variable算子格式不一致的问题。

## 调用示例

完整调用示例请参见[调用示例](aclgrphBundleSaveModel.md#调用示例)。
