# 编译并运行FlowGraph

## 功能介绍

构建完FlowGraph之后，如果您希望直接编译并运行FlowGraph，得到图的执行结果，可以参考本节内容。涉及的主要接口为：

![](figures/260109163843623.png)

1. 调用“GEInitialize”接口进行系统初始化（也可在Graph构建前调用），申请系统资源。
2. 调用“Session构造函数”创建Session类对象，申请Session资源。
3. 调用“FlowGraph”接口在Session类对象中添加定义好的图。
4. 调用“RunGraph”接口或者“FeedDataFlowGraph（feed所有输入）”和“FetchDataFlowGraph（获取所有输出数据）”接口运行图。
5. 调用"GEFinalize"接口，释放系统资源。

> [!NOTE]说明
>如上步骤中的“GEInitialize”、“Session构造函数”、“AddGraph”、“RunGraph”、“GEFinalize”详细信息请参考[《GE图引擎 API》](https://hiascend.com/document/redirect/CannCommunityAscendGraphApi)。
>使用DataFlow开发框架时，NN模型执行使用的是饱和模式。该模式下计算精度可能存在误差，该模式仅为兼容旧版本，后续不演进。

## 开发示例

1. 包含的头文件。

    ```cpp
    #include "ge_api.h"
    ```

2. 申请系统资源。

    Graph定义完成后，调用GEInitialize进行系统初始化（也可在Graph定义前调用），申请系统资源。示例代码如下：

    ```cpp
    std::map<AscendString, AscendString>config = {{"ge.exec.deviceId", "0"},
                                                  {"ge.exec.logicalDeviceClusterDeployMode", "SINGLE"},
                                                  {"ge.exec.logicalDeviceId", "[0:0]"},
                                                  {"ge.graphRunMode", "1"},
                                                  {"ge.exec.precision_mode", "allow_fp32_to_fp16"}};
    Status ret = ge::GEInitialize(config);
    ```

    可以通过config配置传入GE运行的初始化信息，以上配置中的参数（ge.exec.deviceId、ge.graphRunMode和ge.exec.precision\_mode），分别用于指定GE实例运行设备，图执行模式（在线推理请配置为0，训练请配置为1），以及算子精度模式。

    > [!NOTE]说明
    >由于部分UDF不支持负荷分担，所以需要在GE初始化时添加\{"ge.exec.logicalDeviceClusterDeployMode", "SINGLE"\}, \{"ge.exec.logicalDeviceId", "\[0:0\]"\}。其中logicalDeviceId可以是\[0:0\]，也可以是\[0:1\]。如果需要多实例部署，可以参考[指定DataFlow节点部署位置](special_topics.md#指定dataflow节点部署位置)来实现多实例部署。logicalDeviceId解释如下。
    >logicalDeviceClusterDeployMode为SINGLE时，用于指定模型部署在某个指定的设备上。
    >配置格式：\[node\_id:device\_id\]
    >- node\_id：AI处理器逻辑ID，从0开始，表示资源配置文件中第几个设备。
    >- device\_id：AI处理器物理ID。

3. 添加Graph对象并运行Graph。

    若想使定义好的Graph运行起来，首先，要创建一个Session对象，然后调用AddGraph接口添加图，再调用RunGraph接口执行图。示例代码如下：

    ```cpp
    std::map <AscendString, AscendString> options;
    ge::Session *session = new Session(options);
    if(session == nullptr) {
    std::cout << "Create session failed." << std::endl;
    return FAILED;
    }
    // 构造FlowGraph
    Status ret = session->AddGraph(graph_id, flow_graph.ToGeGraph());
    if(ret != SUCCESS) {
    return FAILED;
    }
    // 方式一：RunGraph
    ret = session->RunGraph(graph_id, input, output);
    if(ret != SUCCESS) {
    return FAILED;
    }
    // 方式二：先FeedDataFlowGraph再FetchDataFlowGraph
    ge::DataFlowInfo dataFlowInfo;
    geRet = session->FeedDataFlowGraph(0, input, dataFlowInfo, 3000);
    if (geRet != ge::SUCCESS) {
        return FAILED;
    }
    geRet = session->FetchDataFlowGraph(0, output, dataFlowInfo, 3000);
    if (geRet != ge::SUCCESS) {
        return FAILED;
    }
    ```

    用户可以通过传入options配置图运行相关配置信息。

    其中图运行完之后的数据保存在output中。

    如果DataFlow图中包含UDF节点，DataFlow在图编译过程中会将编译结果放到用户指定UDF的workspace目录下，落盘文件以graph\_name\_release.tar.gz格式存储。该文件用于DataFlow模型部署阶段解压使用，在程序运行期间保证该文件不能被手动删除。

4. 图运行完之后，通过GEFinalize释放资源。

    ```cpp
    ret = ge::GEFinalize();
    ```
