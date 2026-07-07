# Profiling性能数据采集

用户可以在图加载和图执行过程中，采集Profiling性能数据，用于性能分析，本节给出详细的采集方法。

## 功能介绍

<!-- npu="310b" id1 -->
Atlas 200I/500 A2 推理产品不支持该特性。
<!-- end id1 -->

采集Profiling性能数据有两种方式，如下表所示。

**表 1**  Profiling性能数据采集方式

| 序号 | 采集方式 |
| --- | --- |
| 方式一 | 通过[GEInitializeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEInitializeV2.md)传入option参数：<br>  - ge.exec.profilingMode<br>  - ge.exec.profilingOptions<br>该方式采集的性能数据将存放在ge.exec.profilingOptions的output参数所配置的路径下。 |
| 方式二 | 调用如下接口，采集Profiling性能数据：<br>  - [aclgrphProfInit](../../../api/graph_engine_api/cpp/ge/aclgrphProfInit.md)<br>  - [aclgrphProfFinalize](../../../api/graph_engine_api/cpp/ge/aclgrphProfFinalize.md)<br>  - [aclgrphProfCreateConfig](../../../api/graph_engine_api/cpp/ge/aclgrphProfCreateConfig.md)<br>  - [aclgrphProfDestroyConfig](../../../api/graph_engine_api/cpp/ge/aclgrphProfDestroyConfig.md)<br>  - [aclgrphProfStart](../../../api/graph_engine_api/cpp/ge/aclgrphProfStart.md)<br>  - [aclgrphProfStop](../../../api/graph_engine_api/cpp/ge/aclgrphProfStop.md)<br>如果需要采集迭代轨迹数据，还需要通过[GEInitializeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEInitializeV2.md)传入option参数ge.exec.profilingOptions。传入字段包括training_trace/bp_point/fp_point。<br>该方式采集的性能数据将存放在aclgrphProfInit的profiler_path参数所配置的路径下。 |

## 采集前配置

进行Profiling采集前，需要从[graph\_run](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/8_graphrun/graph_run)中获取构建Graph并直接编译运行Graph样例，随后进行如下操作：

1. 在源码文件“main.cpp”开头添加“\#include "ge/ge\_prof.h"”代码。
2. 在编译脚本“Makefile”内的“LIBS”行下添加“-lmsprofiler”字段或在“CMakeLists.txt”内的“target\_link\_libraries”行下添加“msprofiler”字段。

其中头文件ge/ge\_prof.h用于定义Profiling配置的接口，对应的库文件为libmsprofiler.so（头文件在“CANN软件安装后文件存储路径/include/”目录，库文件在“CANN软件安装后文件存储路径/lib64/”目录）。

## 通过方式二采集性能数据

该特性为全局特性，不是Session级特性，开启后在所有Session均配置生效。

建议的接口调用顺序为：

![](../figures/api_call_sequence.png)

调用示例为：

```c++
  // 构造Graph，该步骤省略
  // ...

  // 初始化GE
  std::map<std::string, std::string> ge_options = {{"ge.socVersion", "xxx"}, {"ge.graphRunMode", "1"}};
  ge::GEInitializeV2(ge_options);

 // 配置并启动Profiling
  std::string profilerResultPath = "/home/test/prof";             // 该路径需要提前创建
  uint32_t length = strlen("/home/test/prof");
  ret = ge::aclgrphProfInit(profilerResultPath.c_str(), length); // 初始化Profiling

  // 创建Session并添加 Graph
  std::map<string, string> options = {{"a", "b"}, {"c", "d"}};
  uint32_t graphId = 0;
  ge::Graph graph;
  ge::GeSession *session = new GeSession(options);
  ret = session->AddGraph(graphId, graph);

 // 配置Profiling采样参数
  uint32_t deviceid_list[1] = {0};
  uint32_t device_nums = 1;
  uint64_t data_type_config = ProfDataTypeConfig::kProfTaskTime | ProfDataTypeConfig::kProfAiCoreMetrics | ProfDataTypeConfig::kProfAicpu | ProfDataTypeConfig::kProfTrainingTrace | ProfDataTypeConfig::kProfHccl | ProfDataTypeConfig::kProfL2cache;
  std::vector<ge::Tensor> inputs;
  std::vector<ge::Tensor> outputs;
  ProfAicoreEvents *aicore_events = NULL;
  ProfilingAicoreMetrics aicore_metrics = ProfilingAicoreMetrics::kAicoreArithmeticUtilization;
  ge::aclgrphProfConfig *pro_config = ge::aclgrphProfCreateConfig(deviceid_list, device_nums, aicore_metrics, aicore_events, data_type_config);

  ge::aclgrphProfStart(pro_config);          // 开始Profiling

  session->RunGraph(graphId, inputs, outputs);  // 执行Graph

  ge::aclgrphProfStop(pro_config);            // 停止Profiling数据采集

  // 释放资源
  ge::aclgrphProfDestroyConfig(pro_config);   // 销毁Profiling配置

  ge::aclgrphProfFinalize();                   // 结束Profiling

  delete session;                                 // 释放Session
  ge::GEFinalizeV2();                               // 释放GE相关资源

```
