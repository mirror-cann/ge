# 编译Graph为离线模型

本节给出将Graph编译为om离线模型涉及的主要接口以及调用示例。

## 功能介绍

将Graph编译为离线模型涉及的主要接口为：

![图示](../figures/ir_graph_construction_2.png)

1. Graph定义完成后，调用[aclgrphBuildInitialize](../../../api/graph_engine_api/cpp/ge/aclgrphBuildInitialize.md)进行系统初始化，申请系统资源。
2. 调用[aclgrphBuildModel](../../../api/graph_engine_api/cpp/ge/aclgrphBuildModel.md)将Graph编译为适配AI处理器的离线模型，编译过程中会加载内置算子库和自定义算子库。此时模型保存在内存缓冲区中。
3. （可选）如果希望将内存缓冲区中的模型保存为离线模型文件\*.om，可以调用[aclgrphSaveModel](../../../api/graph_engine_api/cpp/ge/aclgrphSaveModel.md)，序列化保存离线模型到文件中。
4. 调用[aclgrphBuildFinalize](../../../api/graph_engine_api/cpp/ge/aclgrphBuildFinalize.md)结束进程，释放系统资源。

当前支持在一个进程中连续调用模型编译和模型文件保存接口，用于编译和保存多个离线模型。

## 开发示例

1. 包含的头文件。

    ```c++
    #include "ge_ir_build.h"
    #include "ge_api_types.h"
    ```

2. 申请资源。

    创建完Graph以后，通过[aclgrphBuildInitialize](../../../api/graph_engine_api/cpp/ge/aclgrphBuildInitialize.md)接口进行系统初始化，并申请资源。示例代码如下：

    ```c++
    std::map<AscendString, AscendString> global_options = {
            {ge::ir_option::SOC_VERSION, "${soc_version}"},
        };
    auto status = aclgrphBuildInitialize(global_options);
    ```

    可以通过传入**global\_options**参数配置离线模型编译初始化信息，当前支持的配置参数请参见[aclgrphBuildInitialize支持的配置参数](../../../api/graph_engine_api/cpp/ge/aclgrphBuildInitialize_config_params/aclgrphbuildinitialize_config_params.md)。

    若当前环境中不存在AI处理器时，SOC\_VERSION为必选配置，用于指定目标芯片版本，值$\{soc\_version\}需要根据实际情况替换。其他参数请用户根据实际需要可选配置。

3. 通过[aclgrphBuildModel](../../../api/graph_engine_api/cpp/ge/aclgrphBuildModel.md)接口将Graph编译为离线模型。示例代码如下：

    ```c++
    ModelBufferData model;
    std::map<AscendString, AscendString> options;
    PrepareOptions(options);

    ge::Graph graph;
    status = aclgrphBuildModel(graph, options, model);
    if(status == GRAPH_SUCCESS) {
        cout << "Build Model SUCCESS!" << endl;
    }
    else {
        cout << "Build Model Failed!" << endl;
    }
    ```

    可以通过传入**options**参数配置离线模型编译配置信息，当前支持的配置参数请参见[aclgrphBuildModel支持的配置参数](../../../api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/aclgrphbuildmodel_config_params.md)。配置举例：

    ```c++
    void PrepareOptions(std::map<AscendString, AscendString>& options) {
        options.insert({
            {ge::ir_option::EXEC_DISABLE_REUSED_MEMORY, "1"} // close reuse memory
            });
    }
    ```

    > [!NOTE]说明
    >
    >使用aclgrphBuildModel接口传入options参数时，多张图场景下，如果传入的参数为ge::ir\_option::PRECISION\_MODE或者ge::ir\_option::PRECISION\_MODE\_V2，多张图设置的参数值需要相同。

4. （可选）也可以通过[aclgrphSaveModel](../../../api/graph_engine_api/cpp/ge/aclgrphSaveModel.md)将内存缓冲区中的模型保存为离线模型文件。示例代码如下：

    ```c++
    status = aclgrphSaveModel("ir_build_sample", model);
    if(status == GRAPH_SUCCESS) {
            cout << "Save Offline Model SUCCESS!" << endl;
    }
    else {
            cout << "Save Offline Model Failed!" << endl;
    }
    ```

5. 构图进程结束时，通过[aclgrphBuildFinalize](../../../api/graph_engine_api/cpp/ge/aclgrphBuildFinalize.md)接口释放资源。示例代码如下：

    ```c++
    aclgrphBuildFinalize();
    ```

## 后续步骤

- 如果上述编译生成的是om离线模型：

    后续进行推理业务，需要使用**从文件中**加载模型的接口，例如aclmdlLoadFromFile，然后使用aclmdlExecute接口执行推理。

- 如果上述编译生成的离线模型保存在内存缓冲区：

    后续进行推理业务时，需要使用**从内存中**加载模型的接口，例如aclmdlLoadFromMem，然后使用aclmdlExecute接口执行推理。

接口详细介绍请参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理”章节。
