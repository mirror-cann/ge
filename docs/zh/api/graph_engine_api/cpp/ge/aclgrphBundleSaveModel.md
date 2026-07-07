# aclgrphBundleSaveModel

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

将离线模型序列化并保存到指定文件中。

- 与[aclgrphSaveModel](aclgrphSaveModel.md)接口的区别是，该接口适用于权重更新场景，且通过该接口生成的模型，如果要使用acl进行推理，需要使用aclmdlBundleLoadFromFile或aclmdlBundleLoadFromMem接口加载模型，接口详细介绍请参见《GE图引擎 API》中的“模型管理 \> 模型加载和卸载”。
- 通过[aclgrphBundleBuildModel](aclgrphBundleBuildModel.md)接口生成离线模型缓存后才可以使用aclgrphBundleSaveModel接口落盘。

## 函数原型

```c++
graphStatus aclgrphBundleSaveModel(const char_t *output_file, const ModelBufferData &model)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model | 输入 | aclgrphBundleBuildModel生成的离线模型缓冲数据。详情请参见[ModelBufferData](ModelBufferData.md)。<br>其中data指向生成的模型数据，length代表该模型的实际大小。 |
| output_file | 输入 | 保存离线模型的文件名。生成的离线模型文件名会自动以.om后缀结尾，例如ir_build_sample.om或者<br>ir_build_sample_linux_x86_64.om，若om文件名中包含操作系统及架构，则该om文件只能在该操作系统及架构的运行环境中使用。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

若生成的om模型文件名中含操作系统及架构，但操作系统及其架构与模型运行环境不一致时，需要与OPTION\_HOST\_ENV\_OS、OPTION\_HOST\_ENV\_CPU参数配合使用，设置模型运行环境的操作系统类型及架构。参数具体介绍请参见[aclgrphBuildInitialize支持的配置参数](./aclgrphBuildInitialize_config_params/basic_functions.md)。

## 调用示例

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
