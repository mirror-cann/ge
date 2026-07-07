# 权重更新

对于权重更新的场景，为便于用户一次编译模型后，在模型执行阶段能动态更新权重，可通过本章节提供的接口实现该功能。

## 功能介绍

将权重可更新的Graph编译为离线模型，涉及的主要接口为：

![图示](../figures/feature_intro_11.png)

1. Graph定义完成后，调用[aclgrphBuildInitialize](../../../api/graph_engine_api/cpp/ge/aclgrphBuildInitialize.md)进行系统初始化，申请系统资源。
2. 调用[aclgrphConvertToWeightRefreshableGraphs](../../../api/graph_engine_api/cpp/ge/aclgrphConvertToWeightRefreshableGraphs.md)生成权重可更新的图，包括三部分：权重初始化图，权重更新图，推理图。
3. 调用[aclgrphBundleBuildModel](../../../api/graph_engine_api/cpp/ge/aclgrphBuildModel.md)将权重更新的Graph编译为适配AI处理器的离线模型，编译过程中会加载内置算子库和自定义算子库。此时模型保存在内存缓冲区中。
4. 如果希望将内存缓冲区中的模型保存为离线模型文件xx.om，需要调用[aclgrphBundleSaveModel](../../../api/graph_engine_api/cpp/ge/aclgrphBundleSaveModel.md)，序列化保存离线模型到文件中。
5. 调用[aclgrphBuildFinalize](../../../api/graph_engine_api/cpp/ge/aclgrphBuildFinalize.md)结束进程，释放系统资源。

当前支持在一个进程中连续调用模型编译和模型文件保存接口，用于编译和保存多个离线模型。

## 使用方法

```c++
// 包含的头文件：
#include "ge_ir_build.h"
#include "ge_api_types.h"

// 1.生成Graph
Graph origin_graph("Irorigin_graph");
GenGraph(origin_graph);

// 2.创建完Graph以后，通过aclgrphBuildInitialize接口进行系统初始化，并申请资源
std::map<AscendString, AscendString> global_options;
auto status = aclgrphBuildInitialize(global_options);

// 3. 生成权重可更新的图，包括三部分：权重初始化图，权重更新图，推理图
// 权重初始化是可选步骤，根据业务场景由用户判断是否需要包含权重初始化图，不包含的情况下，可节省模型加载所需的Device内存
WeightRefreshableGraphs weight_refreshable_graphs;
std::vector<AscendString> const_names;
const_names.emplace_back(AscendString("const_1"));
const_names.emplace_back(AscendString("const_2"));
status = aclgrphConvertToWeightRefreshableGraphs(origin_graph, const_names, weight_refreshable_graphs);
if (status != GRAPH_SUCCESS) {
    cout << "aclgrphConvertToWeightRefreshableGraphs failed!" << endl;
    aclgrphBuildFinalize();
    return -1;
}

// 4.将权重可更新的图，编译成离线模型，并保存在内存缓冲区中
std::map<AscendString, AscendString> options;
std::vector<ge::GraphWithOptions> graph_and_options;
// 推理图
graph_and_options.push_back(GraphWithOptions{weight_refreshable_graphs.infer_graph, options});
// 权重初始化图
graph_and_options.push_back(GraphWithOptions{weight_refreshable_graphs.var_init_graph, options});
// 权重更新图
graph_and_options.push_back(GraphWithOptions{weight_refreshable_graphs.var_update_graph, options});
ge::ModelBufferData model;
status = aclgrphBundleBuildModel(graph_and_options, model);
if (status != GRAPH_SUCCESS) {
    cout << "aclgrphBundleBuildModel failed" << endl;
    aclgrphBuildFinalize();
    return -1;
}

// 5.将内存缓冲区中的模型保存为离线模型文件
const char *file="./xxx" ;
status = aclgrphBundleSaveModel(file, model);
if (status != GRAPH_SUCCESS) {
    cout << "aclgrphBundleSaveModel failed" << endl;
    aclgrphBuildFinalize();
    return -1;
}
// 6.构图进程结束，释放资源
aclgrphBuildFinalize();
```

## 后续步骤

上述权重更新后的图编译生成的离线模型，如果要进行推理业务，则必须通过aclmdlBundleLoadFromFile或aclmdlBundleLoadFromMem接口加载模型，然后使用aclmdlExecute接口执行推理，详细介绍请参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理 \> 权重更新”章节。
