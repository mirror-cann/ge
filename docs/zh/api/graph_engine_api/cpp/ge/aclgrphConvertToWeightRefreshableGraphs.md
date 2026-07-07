# aclgrphConvertToWeightRefreshableGraphs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

通过传入Const节点名数组将原图转换成一组权重可更新的图。

该接口适用于权重更新场景，详细介绍请参见[专题\>权重更新](../../../../user_guides/graph_dev/more_features/weight_update.md)。

## 函数原型

```c++
graphStatus aclgrphConvertToWeightRefreshableGraphs(const ge::Graph &origin_graph, const std::vector<AscendString> &const_names, WeightRefreshableGraphs &weight_refreshable_graphs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| origin_graph | 输入 | 需要权重更新的原图。 |
| const_names | 输入 | 需要权重更新的节点名数组。 |
| weight_refreshable_graphs | 输出 | 权重可更新的图，该参数为一结构体，包括三部分：权重初始化图，权重更新图，推理图。内容详见如下结构体示例。 |

```c++
struct WeightRefreshableGraphs {
  ge::Graph infer_graph;
  ge::Graph var_init_graph;
  ge::Graph var_update_graph;
};
```

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 调用示例

完整调用示例请参见[调用示例](aclgrphBundleSaveModel.md#调用示例)。
