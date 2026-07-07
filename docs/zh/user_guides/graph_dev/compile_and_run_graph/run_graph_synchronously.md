# 同步运行Graph

本节介绍同步运行Graph涉及的主要接口以及调用示例。

## 功能介绍

构建完Graph之后，如果您希望**直接编译并运行Graph，得到图的运行结果**，可以参考本节内容。涉及的主要接口如下，编译并运行Graph的Session样例可参考[graph\_run](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/8_graphrun/graph_run)。

![图示](../figures/feature_intro_3.png)

1. 调用[GEInitializeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEInitializeV2.md)进行系统初始化（也可在Graph构建前调用），申请系统资源。
2. 调用[GeSession构造函数](../../../api/graph_engine_api/cpp/ge/GeSession/GESession.md)创建Session类对象，申请Session资源。
3. 调用[AddGraph](../../../api/graph_engine_api/cpp/ge/GeSession/AddGraph.md)在Session类对象中添加定义好的图。
4. 调用[RunGraph](../../../api/graph_engine_api/cpp/ge/GeSession/RunGraph.md)运行图。
5. 调用[GEFinalizeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEFinalizeV2.md)，释放系统资源。

## 开发示例

1. 包含的头文件。

    ```c++
    #include "ge_api_v2.h"
    ```

2. 申请系统资源。

    Graph定义完成后，调用GEInitializeV2进行系统初始化（也可在Graph定义前调用），申请系统资源。示例代码如下：

    ```c++
    std::map<AscendString, AscendString>config = {{"ge.exec.deviceId", "0"},
                                                  {"ge.graphRunMode", "1"}};
    Status ret = ge::GEInitializeV2(config);
    ```

    可以通过config配置传入ge运行的初始化信息，配置参数ge.exec.deviceId和ge.graphRunMode，分别用于指定GE实例运行设备，图执行模式（在线推理请配置为0，训练请配置为1）。更多配置请参考[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/options_parameters_description.md)。

3. 添加Graph对象并运行Graph。

    若想使定义好的Graph运行起来，首先，要创建一个Session对象，然后调用AddGraph接口添加Graph，再调用RunGraph接口运行Graph。示例代码如下：

    ```c++
    std::map<AscendString, AscendString>options;
    // 创建Session对象
    ge::GeSession *session = new GeSession(options);
    // 判断Session是否创建成功
    if(session == nullptr) {
      std::cout << "Create session failed." << std::endl;
      // ...
      // ...
      // 释放资源
      ge::GEFinalizeV2();
      return FAILED;
    }
    // 准备将要添加到Session的Graph ID，并创建空的Graph对象
    uint32_t conv_graph_id = 0;
    ge::Graph conv_graph;

    // 将Graph添加到Session
    Status ret = session->AddGraph(conv_graph_id, conv_graph);
    if(ret != SUCCESS) {
      // ...
      // ...
      // 释放资源
      ge::GEFinalizeV2();
      delete session;
      return FAILED;
    }
    // 准备输入输出张量
    std::vector<gert::Tensor> input_cov;
    std::vector<gert::Tensor> output_cov;

    // 运行指定ID的Graph
    ret = session->RunGraph(conv_graph_id, input_cov, output_cov);
    if(ret != SUCCESS) {
      // ...
      // ...
      // 释放资源并销毁Session
      ge::GEFinalizeV2();
      delete session;
      return FAILED;
    }
    ```

    用户可以通过传入options配置图运行相关配置信息，相关配置请参考[Session构造函数](../../../api/graph_engine_api/cpp/ge/GeSession/GESession.md)。

    其中图运行完之后的数据保存在Tensor output\_cov中。

4. 图运行完之后，通过GEFinalizeV2释放资源。

    ```c++
    ret = ge::GEFinalizeV2();
    ```
